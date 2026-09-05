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

手动 Release 只接受已有 draft tag，读取该提交内的清单。变更词库需要新的源码提交和产品版本，不允许临时覆盖词库 tag。没有清单的历史版本不能用新流水线宣称可复现重建。

发布前 `product_lock.py verify-published` 要求清单里每个提交都能从各自仓库的默认分支到达。这一条只在发布路径上执行，不进 `ci.yml`：引擎改动和本仓改动同时落地时，PR 里锁住引擎的分支提交是正常且必要的，在 PR CI 上强制会把它本该保护的那次落地直接锁死。到了发布这一刻判断相反——没进默认分支的输入就是没人合过的输入，锁文件为它背书等于宣称了一次并不存在的评审。

安装包携带 `%LOCALAPPDATA%/metasequoiaime/product-manifest.json`，Release 同时附加同名文件。其中包含本仓的发布提交、全部锁定输入和清单文件摘要。签名时间、工具链与 runner 更新仍可能改变二进制字节；此机制保证产品源码与数据组合确定，不承诺安装包逐字节一致。

本地检查：

```sh
python -m unittest discover -s tests -p 'test_product_lock.py'
python scripts/product_lock.py verify-contracts .
python scripts/product_lock.py fetch-dictionaries --staging-root /tmp/msime-product-data
```
