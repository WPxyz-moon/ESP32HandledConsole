# ESP32HandledConsole

项目背景与硬件口径见 [README.md](README.md) 和 [docs/README.md](docs/README.md)。

## 核心工作流：自动提交与工作树调度

每次完成对代码文件的任何修改（创建、编辑、删除）后，必须严格执行以下原子化工作流：

### 1. 提交与推送 (Commit & Push)

- 立即使用 `git add <修改的文件>` 暂存变更。
- 使用 `git commit -m "<type>(<scope>): <简要描述>"` 进行提交。
- 使用 `git push origin <当前分支>` 推送到远程仓库。

### 2. 工作树调度 (Worktree Dispatch)

在完成上述提交推送后，必须检查当前 Git Worktree 的状态，并按以下优先级进行调度：

- **标记当前工作树为使用中**：在开始修改代码前，必须执行 `git worktree lock <当前工作树路径>` 锁定当前工作树。
- **识别无人使用的工作树**：
  1. 执行 `git worktree list --porcelain` 获取所有工作树状态。
  2. 解析输出，**不包含 `locked` 标签**的工作树即视为“无人使用（Idle）”状态。
- **调度逻辑**：
  - **若当前工作树仍在修改中**：寻找一个“无人使用”的工作树，执行 `git worktree lock` 锁定它，并切换至该目录继续后续操作。
  - **若所有工作树均在使用中（即均包含 `locked` 标签）**：新建一个工作树 `git worktree add ../<new-name> -b <new-branch>`，执行 `git worktree lock` 锁定它，并切换至新工作树。
- **释放工作树**：当某个任务彻底完成且不再需要修改时，执行 `git worktree unlock <路径>` 将其释放为“无人使用”状态。

> 实施备注：Git 不允许锁定主工作树（`fatal: The main working tree cannot be locked or unlocked`）。
> 当前仓库默认只有一个主工作树；需要执行上述锁定/调度流程时，先按第 2 节新建一个
> linked worktree 并在其中工作，不要尝试锁定仓库根目录。
