# Windows 产品版本组合

一个 Windows 版本的完整一方源码就是本仓的一个提交：TSF、Server、GUI 框架、页面和安装器都是目录，不需要再被清单钉住。

`product-lock.json` 只覆盖仍来自仓外的输入：Engine 的提交，以及词库 Release 的 source commit 和每个产物的 SHA256。辅助码已并入 Engine，由同一个 gitlink 钉住，不再单列。Engine 在本仓是 `vendor/MetasequoiaImeEngine` submodule，gitlink 才是权威；清单里的 `engine.commit` 是它的记录副本，供产物清单和发布门禁使用，`product_lock.py verify-contracts` 保证两者一致。

发布任务不解析 `main` 或 `latest`。递归 submodule 沿固定 gitlink 检出，不执行 `git submodule update --remote`。

## 更新依赖

显式刷新后评审并提交清单，CI 通过后才能发布：

```sh
python scripts/product_lock.py refresh --dictionary-tag dict-v1.0.0
python scripts/product_lock.py validate
```

`refresh` 不再解析任何浮动源码引用，`--ref` 已无可覆盖的对象。Engine 不从默认分支解析：bump submodule 本身就是那次评审，`refresh` 只把 gitlink 抄进清单，不会把清单指向一个没被集成的引擎版本。

词库 tag 本身可能被上游修改，因此构建验证的是本清单中已提交的摘要，连同模型授权文件和上游校验文件一起验证。只更新上游 SHA256SUMS 无法让被替换的数据通过构建。词库清单另外声明构建它的 commit 和当时工作树是否干净，两者都要与清单相符——摘要证明字节没被换过，证明不了这批数据出自一个能重建的源。

## 重建与追溯

自动路径构建的是 release-please 刚刚建出的那个 draft，`resolve-auto-release.sh` 要求它钉在完整 commit SHA 上；手动 Release 只接受已有 draft tag，由 `validate-draft-release.sh` 做同样的校验。两条路径都只读该提交内的清单。变更词库需要新的源码提交和产品版本，不允许临时覆盖词库 tag。没有清单的历史版本不能用新流水线宣称可复现重建。

## 发布节奏

发版由把 `develop` 提升到 `main` 触发；提升之后是全自动的，`main` 上每一次带 `fix:` / `feat:` 的合并都会走完构建、签名、发布。

**签名额度不是约束。** 证书允许每月 3000 次签名，而这个项目一个月的发布次数远低于这个量级。此前把「省签名次数」当作攒版本理由的说法不成立，不要据此调整合并策略。

**真正花掉的是发布节奏本身。** 自动链条一旦起跑就没有人工落点，「把若干修复攒成一个版本」和「这次不值得发」这类判断必须发生在它起跑之前。`develop` 就是这个落点：日常修复合进 `develop` 不产生任何版本，攒够一批再一次性提升到 `main`，那批提交会落在同一次 push 上、合成同一个版本。要让某次提升完全不递增版本，把提交类型写成 `chore:` / `docs:` / `refactor:` / `test:` 即可。

这一段原本记录的是相反的处境：默认分支曾经就是 `main`，消耗版本号的决定权因此落在「有没有人合了一个 `fix:`」上，用户会看到一串只差一两个提交的版本，release notes 也跟着碎。`develop` 模型把那个决定权收了回来。

**换的是什么。** 从合并到安装包可下载之间不再有人工环节，也就不会出现改动躺在 draft 里没人发的情况。

**排队。** Release runs 按 `concurrency: release-${{ github.ref }}` 串行且不取消。一轮完整的自动发布 = 等 release PR 的 CI（十几分钟）+ 构建签名打包（二十分钟上下）。向 `main` 提升的频率高于这个节奏时队列只会越排越长，每个排队的 run 到头来都发一个版本。真出现这种情况，该调的是提升节奏或提交类型，不是把 `cancel-in-progress` 打开——那会在发布中途砍掉 run，留下挂着半个产物的 draft。

发布前 `product_lock.py verify-published` 要求清单里每个提交都能从各自仓库的默认分支到达，词库的 `source_commit` 一并检查——它和 Engine 提交一样钉住仓外的输入，而摘要与词库清单只能证明这批数据出自锁文件写着的那个提交，证明不了那个提交有人合过。唯一的例外是已退休的词库仓库：它的历史 Release 是不可变的既有产物，仓库归档或删除后无从追溯默认分支，此时只记录不拦截。这一条只在发布路径上执行，不进 `ci.yml`：引擎改动和本仓改动同时落地时，PR 里锁住引擎的分支提交是正常且必要的，在 PR CI 上强制会把它本该保护的那次落地直接锁死。到了发布这一刻判断相反——没进默认分支的输入就是没人合过的输入，锁文件为它背书等于宣称了一次并不存在的评审。各代码仓的默认分支现在是 `develop`，这条判据本身没有变：进了 `develop` 就是有人评审合过，发布提升与它无关。

安装包携带 `%LOCALAPPDATA%/metasequoiaime/product-manifest.json`，Release 同时附加同名文件。其中包含本仓的发布提交、全部锁定输入和清单文件摘要。签名时间、工具链与 runner 更新仍可能改变二进制字节；此机制保证产品源码与数据组合确定，不承诺安装包逐字节一致。

本地检查：

```sh
python -m unittest discover -s tests -p 'test_product_lock.py'
python scripts/product_lock.py verify-contracts .
python scripts/product_lock.py fetch-dictionaries --staging-root /tmp/msime-product-data
```

## 发布元数据令牌

版本 PR 的创建和更新使用 `RELEASE_PLEASE_TOKEN`，使正常 `pull_request` CI 自动运行并进入 PR 检查汇总。`land-release-pr.sh` 等待精确 head 的 PR CI 通过，再用 `GITHUB_TOKEN` 合并。维护 PR 的 release-please 调用设置 `skip-github-release: true`。它前后各有一次 `skip-github-pull-request: true` 的元数据调用，使用组织已有的 `RELEASE_PLEASE_TOKEN`：先补建已经手动合并的版本，再处理本次自动合并的版本，保持 release-please 原有的先发布、后计算下一版本的顺序。

后者需要仓库内容和 workflow 写权限（经典 PAT 的 `repo` + `workflow`，或等价的细粒度权限）。主分支先前包含 workflow 变更时，`GITHUB_TOKEN` 创建历史提交的 tag 可能被 Git refs API 拒绝。该令牌只更新版本分支，不用于合并；主分支仍由 `GITHUB_TOKEN` 写入，因此不会额外触发一次 Release。缺少令牌时提前报出配置错误。
