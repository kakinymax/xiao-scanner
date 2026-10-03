# 整理前データの復元

保全場所: `C:\Users\pc\Documents\Codex\backups\xiao-scanner\2026-10-03`

配布用ZIPも作成済み。SHA256はSTATUS.mdに記録。バックアップにはGit bundle、整理前のPR/ブランチ/Actions情報、PR別patch、Drive原本、Driveのサイズ/SHA256一覧を含む。私的な一時ダウンロードURLや認証トークンは含めない。

## Git履歴

全107 PR headと105ブランチを含むbundleを、別リポジトリに実際に復元し、git fsckと全107 PR参照を確認済み。

```powershell
$backupRoot = 'C:\Users\pc\Documents\Codex\backups\xiao-scanner\2026-10-03'
git bundle verify "$backupRoot\backup\xiao-scanner-before.bundle"
git clone --mirror "$backupRoot\backup\xiao-scanner-before.bundle" restored-xiao-scanner.git
git --git-dir=restored-xiao-scanner.git fsck --full
```

復元先は空の新しい場所を使う。PRの固有履歴はrefs/archive/pull/番号/headにあり、ブランチ名とSHAはgithub-branches-before.jsonにある。削除済みブランチが必要になった場合は、台帳のSHAをこの復元リポジトリから確認し、そのコミットを対象名のrefs/headsへ戻す。GitHubへの復元は書き込み認証後に行う。

## Drive作業コピー

drive-source/に二つの作業コピー49ファイル、ライブラリの版情報5件、HEAD/main/packed-refs5件を保存。drive-download-manifest.jsonの各SHA256と原本のサイズを照合する。ZIP内の59ファイルも原本のSHA256と一致することを確認した。

これらは旧時点の保全データ。現行作業フォルダへ一括上書きせず、drive-comparison.jsonとPR台帳を使って必要な独自差分だけを復元する。Driveの別コピーのmainはfc1e9f1155e7c2b37e77f689116f15a3ee4027b5、主作業コピーは4e0392659ed472a34eaa94aa4e7875163b9e23e6。いずれも現在のmainより古い。
