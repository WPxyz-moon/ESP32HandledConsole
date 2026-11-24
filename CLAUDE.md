# ESP32HandledConsole

项目背景与硬件口径见 [README.md](README.md) 和 [docs/README.md](docs/README.md)。

## 核心工作流：自动提交、工作树调度与主工作树合并

每次完成对代码文件和相关文档文件的任何修改（创建、编辑、删除）后，必须严格执行以下原子化工作流：

### 1. 提交与推送 (Commit & Push)

- 立即使用 `git add <修改的文件>` 暂存变更。
- 使用 `git commit -m "<type>(<scope>): <简要描述>"` 进行提交。
- 使用 `git push origin <当前分支>` 推送到远程仓库。
- 若后续 rebase 改写了已推送的提交，使用 `git push --force-with-lease origin <当前分支>` 更新远端分支；不要使用裸 `--force`。

### 2. 工作树调度 (Worktree Dispatch)

在完成上述提交推送后，必须检查当前 Git Worktree 的状态，并按以下优先级进行调度：

- **标记当前工作树为使用中**：
  - **linked worktree**：在开始修改代码前，执行 `git worktree lock <当前工作树路径>` 锁定当前工作树。
  - **主工作树**：Git 不允许 `git worktree lock`，不要尝试锁定仓库根目录；主工作树只用于合并，合并期间用 `.merge_lock` 标记（见第 4 节）。
- **识别无人使用的工作树**：
  1. 执行 `git worktree list --porcelain` 获取所有工作树状态。
  2. **linked worktree**：不包含 `locked` 标签的，视为“无人使用（Idle）”。
  3. **主工作树**：不包含 `.merge_lock` 文件的，视为“无人使用（Idle）”。
- **调度逻辑**：
  - **若当前工作树仍在修改中**：寻找一个“无人使用”的工作树，执行 `git worktree lock` 锁定它，并切换至该目录继续后续操作。
  - **若所有工作树均在使用中**（linked worktree 均含 `locked` 标签，且主工作树含 `.merge_lock`）：新建一个工作树 `git worktree add ../<new-name> -b <new-branch>`，执行 `git worktree lock` 锁定它，并切换至新工作树。
- **释放工作树**：当某个任务彻底完成且不再需要修改时，执行 `git worktree unlock <路径>` 将其释放为“无人使用”状态。

> 实施备注：Git 不允许锁定主工作树（`fatal: The main working tree cannot be locked or unlocked`）。
> 当前仓库默认只有一个主工作树；需要执行上述锁定/调度流程时，先按第 2 节新建一个
> linked worktree 并在其中工作，不要尝试锁定仓库根目录。

### 3. 子工作树：先 rebase，再请求合并

子工作树完成提交后，不要直接把分支丢给主工作树；**冲突必须在子工作树内部消化**：

1. `git fetch origin`
2. `git rebase origin/main`
3. 若有冲突，在子工作树内解决：编辑冲突文件 → `git add <file>` → `git rebase --continue`。
4. rebase 完成后，构建/测试通过再推送；若 rebase 改写了已推送历史，用 `git push --force-with-lease origin <当前分支>`。
5. 若冲突无法解决或 rebase 失败：执行 `git rebase --abort`，不要进入主工作树，并通知使用者“分支未合并”及错误原因。

### 4. 主工作树：串行合并与 `.merge_lock`

主工作树用于工作树内容合并，只负责把子工作树的内容合并进 `main` 并推送，不承载日常开发。

- **占用标记**：主工作树当前是否正在合并，由仓库根目录的 `.merge_lock` 文件标记。
  **主工作树不存在 `.merge_lock` 即视为“无人使用（Idle）”；存在即视为“正在合并”。**
- **进入前必须加锁**：切换到主工作树之前，必须先原子创建 `.merge_lock`。推荐在 Git Bash 中执行：

  ```bash
  set -C
  : > /path/to/main-worktree/.merge_lock
  ```

  创建失败说明已有其他 agent 在合并，进入等待；不要删除别人创建的锁。
- **等待规则**：若 `.merge_lock` 已存在，说明主工作树还在合并中；必须等待其结束（轮询检查文件是否消失），确认合并结束、文件已删除后，再加锁、合并并推送。默认等待上限 10 分钟，可按任务调整。
- **合并步骤**（在持有 `.merge_lock` 后执行）：

  1. `cd <主工作树>`
  2. `git fetch origin`
  3. `git checkout main`
  4. `git pull --ff-only origin main`
  5. `git merge --no-ff <子工作树分支>`
  6. 运行必要的构建/测试
  7. `git push origin main`

- **冲突处理**：子工作树应已 rebase 到最新 `origin/main`，主工作树合并理论上应无冲突。若主工作树仍出现冲突，**不要在主工作树里手工解决**；执行 `git merge --abort`，删除 `.merge_lock`，通知使用者“尚未合并”及冲突文件/错误原因，让子工作树重新 rebase 解决。
- **释放锁**：无论合并成功、失败、超时还是中止，任务结束后都必须删除自己创建的 `.merge_lock`：

  ```bash
  rm -f /path/to/main-worktree/.merge_lock
  ```

- **等待过久或无法解决**：若等待主工作树释放锁超时，或合并冲突无法解决，必须中止合并（`git merge --abort` / `git rebase --abort`），删除 `.merge_lock`，然后通知 agent 使用者：**尚未合并**，并说明错误原因（超时、冲突文件、构建失败等）。
- **不要提交 `.merge_lock`**：它只是运行时锁文件，不要 `git add`。
