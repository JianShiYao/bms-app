## 变更说明
<!-- 这个 PR 做了什么、为什么 -->

关联 issue：<!-- Refs #n / 无 -->

## 变更类型
<!-- 勾选一项 -->
- [ ] feat（新功能）
- [ ] fix（缺陷修复）
- [ ] docs（文档）
- [ ] ci / build（流水线 / 构建）
- [ ] refactor / style / test / chore（重构 / 格式 / 测试 / 杂项）

## 自检清单
- [ ] `scripts\format.ps1 -Check` 通过
- [ ] `west build -b mps2/an386 app` 编译通过
- [ ] `west twister -T tests -p mps2/an386 -c` 通过，无失败用例
- [ ] 提交信息符合 Conventional Commits
- [ ] 追溯链无断链（REQ→DES→code→test；如属特性小 V，`docs/features/<slug>/traceability.md` 已更新）
- [ ] 安全相关改动：已关联安全需求并有失效安全测试（不涉及则划掉）
- [ ] 如涉及发版：已更新 `VERSION` 与 `CHANGELOG.md`
- [ ] 如涉及接口/行为：已更新相关文档

## 风险与回滚（安全相关变更必填）
<!-- BMS 保护/接触器/采样等安全相关改动：风险点、验证方式、回滚办法 -->
