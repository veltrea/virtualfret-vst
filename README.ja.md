**他の言語で読む:** [English](README.md)

# VirtualFret

ギターの指板を再現したスクリーン MIDI キーボード(VST3 プラグイン)。指板のクリックで単音、ラッチでコードを組み、ストラムゾーンのドラッグで時間差のあるストロークを演奏 — すべて MIDI として下流のギター音源へ送ります。VirtualFret 自体は音を出しません。

鍵盤ではなく指板の配置で考えるギタリストのためのツールです。覚えているコードシェイプのまま、ギター音源・モデリング音源へ MIDI を打ち込めます。

## 特徴

- **6 / 7 / 8 / 9 弦**の指板(開放 + 24 フレット、タブ譜と同じ向き: 最上段 = 1 弦)
- **単音演奏**: セルを押す = note on、離す = note off。弦に沿った / 跨いだドラッグで音を差し替え(実ギターと同じ 1 弦 1 音)
- **チューニング**: 内蔵プリセット 17 種(Standard、Drop D/C#/C/B、半音下げ、D Standard、Open G/D/E/A、DADGAD、7/8/9 弦セット)、ヘッド部での弦別半音編集、ユーザーチューニングの共有ファイル保存
- **コードモード**: 弦ごとに 1 ポジションをラッチ(フレット / 開放 ○ / ミュート ×)、ラッチ時試聴、状態は DAW プロジェクトと一緒に保存
- **コードプリセット**: ルート × 13 タイプ(maj, min, 7, maj7, m7, sus4, sus2, add9, 9, m9, dim, aug, 5)をシェイプライブラリから解決。フォーム(E フォーム / A フォーム / オープン)をボタン 1 つで巡回。自分のボイシングも保存可能
- **ストラムゾーン**: 弦線を跨ぐドラッグでストローク。跨ぎ速度 = ベロシティ、跨いだ時刻は大きいバッファサイズでもサンプル精度で再現。let ring 動作。弦を跨がないクリック = 全消音
- **キーボードストラム**: Space = ダウンストローク、Shift+Space = アップストローク、Esc = 全消音
- **チャンネルモード**: 単一チャンネル、または弦別チャンネル(弦別対応のギター音源向け)
- **入力 MIDI ハイライト**: 受信ノートを鳴らせる全ポジションを指板上で点灯
- **コンパクト表示**: 1 画面に表示するフレット数を設定から選択(12 / 15 / 18 / 21 / 24)。ウィンドウは Guitar Pro 風の細いフレットボード帯まで縮小可能
- **左利きモード**: 設定から指板全体を左右ミラー(ヘッドが右、ストラムゾーンが左)
- **UI 言語**: English / 日本語(OS に追従、設定から切替可能)

## 動作環境

- macOS 12+(Universal: Apple Silicon + Intel)。Windows 対応は後続予定。
- VST3 ホスト(Ableton Live、REAPER、Studio One など)
- VST3 + Standalone のみ。AU 版は意図的にありません: Ableton Live は AU プラグインの MIDI 出力をルーティングできないためです。

## インストール

1. GitHub Releases から `VirtualFret-<version>-macOS.zip` をダウンロードして解凍。
2. `VirtualFret.vst3` を `~/Library/Audio/Plug-Ins/VST3/` にコピー。
3. プラグインは ad-hoc 署名です。DAW がロードを拒否する場合:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/VirtualFret.vst3
```

## DAW セットアップ

### Ableton Live — 必ず最初に読んでください

VirtualFret は MIDI を**生成**するプラグインで、Live で生成 MIDI を受けるには**デバイスタップ**の選択が必須です。2 トラック構成にします:

```
Track A                          Track B
+--------------------+          +----------------------------+
| VirtualFret        |          | your guitar instrument     |
| (as an instrument) |          |                            |
+--------------------+          | MIDI From:                 |
                                |   [A-VirtualFret      v]   |  <- track A
                                |   [VirtualFret        v]   |  <- the DEVICE TAP,
                                |                            |     NOT "Post FX"
                                | Monitor: [In]              |
                                +----------------------------+
```

1. トラック A に VirtualFret を挿す(インストゥルメントとして表示されます)。
2. トラック B の **MIDI From** をトラック A にし、**2 段目のセレクタで「VirtualFret」**(デバイスタップ)を選びます。**「Post FX」では動きません**: Post FX に乗るのはスルーした入力 MIDI だけで、プラグインが生成したノートは含まれません。
3. トラック B の **Monitor を In** にします。

「クリックしても鳴らないのに、入力 MIDI のスルーは通る」場合は、2 段目が「Post FX」のままです。デバイスタップに切り替えてください。

### Studio One

音源の **Note FX** スロットに VirtualFret を挿します。

### REAPER

FX チェーンの音源より前に VirtualFret を挿します(MIDI はチェーンを流れます)。

## 指板の使い方

- **演奏**: セル(弦 × フレット)をクリック。ナット左の開放弦ゾーンで開放弦が鳴ります。ドラッグでフレット間・弦間をスライド。
- **チューニング**: ヘッド部の音名をクリック(上半分 = 半音上げ、下半分 = 半音下げ)、または縦ドラッグ。指板の見た目は変わらず、各セルの音程だけが変わります。チューニングセレクタ横の `...` ボタンから保存。
- **コード**: コードモードを ON にしてセルをクリックでラッチ(再クリックで解除。ラッチのない弦はミュート ×)。またはツールバーでルート + タイプを選び、フォームボタンで巡回。横の `...` ボタンからボイシングを保存。
- **ストラム**: 右端ゾーンで弦線を跨ぐようにドラッグ。速いほど強く鳴ります。弦を跨がないクリックは全消音(ピックミュート)。ストラムした音は同じ弦が次に鳴るまで持続します。
- **キー操作**: Esc = 全消音。Space / Shift+Space = ダウン / アップストローク。

## ユーザーファイル

全インスタンス・全プロジェクトで共有されます:

- `~/Library/Application Support/VirtualFret/tunings.json`
- `~/Library/Application Support/VirtualFret/chords.json`

手書きで追加しても読み込めます。チューニングの形式:

```json
{ "name": "My Drop C", "strings": [36, 43, 48, 53, 57, 62] }
```

(`strings` は低音弦 → 高音弦の順、弦ごとの開放弦 MIDI ノート番号。)

## ソースからのビルド

```bash
git clone https://github.com/veltrea/virtualfret-vst.git
cd virtualfret-vst
# Place JUCE 8 at external/JUCE (it is not committed):
#   git clone --depth 1 https://github.com/juce-framework/JUCE external/JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target VirtualFret_VST3 VirtualFret_Standalone CoreTests -j8
ctest --test-dir build            # core logic tests
bash scripts/install-plugins.sh   # refuses to run while Live is open
```

`scripts/package-release.sh` で ad-hoc 署名済みのリリース zip と dSYM を生成します。

## 補足

- 入力 MIDI はすべて無改変でスルーし、生成ノートをストリームにマージします。
- エディタクローズ・再生停止・チューニング変更・弦数変更では発音中のノートを必ず解放します — 設計上スタックノートは発生しません。
