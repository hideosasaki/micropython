# MicroPython RP2040-Zero Deep Sleep機能 Pythonリファレンス

## 概要
このドキュメントは、RP2040-Zero向けに追加したDeep Sleep機能（`picosleep`モジュール）のPython APIの使い方をまとめたものです。

---

## 1. モジュールのインポート
```python
import picosleep
```

---

## 2. 主な関数一覧

### 2.1. deepsleep(ms)
RP2040-Zeroを指定したミリ秒だけDeep Sleep（低消費電力状態）に移行します。

#### 使い方例
```python
import picosleep

# 5000ミリ秒（5秒）Deep Sleep
picosleep.deepsleep(5000)
```

#### 引数
- `ms` : Deep Sleepする時間（ミリ秒, int型）

#### 備考
- Deep Sleep中はCPUが停止し、消費電力が大幅に低下します。
- Deep Sleep中はRAM内容が保持されます。
- 指定時間経過後、自動的に復帰します。
- 復帰後は通常のMicroPythonスクリプトの続きが実行されます。
- ms=0を指定すると無限Deep Sleepとなり、GPIOピンによる復帰のみ可能です。

### 2.2. set_wakeup_pin(pin)
Deep Sleepからの復帰トリガーとなるGPIOピン番号を設定します。

#### 使い方例
```python
import picosleep

# GPIO15をWakeupピンに設定
picosleep.set_wakeup_pin(15)
```

#### 引数
- `pin` : GPIOピン番号（int型、例: 15）

#### 備考
- 複数回呼び出すことでピンを変更可能です。

### 2.3. wake_reason()
Deep Sleepから復帰した理由を取得します。

#### 使い方例
```python
import picosleep

reason = picosleep.wake_reason()
if reason == picosleep.WAKE_TIMER:
    print("Wake reason: timer")
elif reason == picosleep.WAKE_GPIO:
    print("Wake reason: gpio")
else:
    print("Wake reason: unknown")
```

#### 戻り値
- 復帰理由を示す整数（int型）。
    - picosleep.WAKE_TIMER (0): タイマー復帰
    - picosleep.WAKE_GPIO  (1): GPIOピン復帰


---

## 3. 応用例

### 3.1. Deep Sleepからの復帰理由取得（定数で判定）
```python
import picosleep

# GPIO15をWakeupピンに設定
picosleep.set_wakeup_pin(15)

# 5000ミリ秒（5秒）Deep Sleep
picosleep.deepsleep(5000)

reason = picosleep.wake_reason()
if reason == picosleep.WAKE_TIMER:
    print("Wake reason: timer")
elif reason == picosleep.WAKE_GPIO:
    print("Wake reason: gpio")
else:
    print("Wake reason: unknown")
```


### 3.2. ボタンクリックで復帰するDeep Sleep
```python
import picosleep

# GPIO15をWakeupピンに設定
picosleep.set_wakeup_pin(15)

# ボタン（GPIO15）が押されたら復帰
picosleep.deepsleep(0)

print("Wakeup by button click!")
```

### 3.3. ボタン長押しで復帰するDeep Sleep
```python
import picosleep
import machine
import time

BUTTON_PIN = 15
picosleep.set_wakeup_pin(BUTTON_PIN)

while True:
    # Deep Sleep（ボタンで復帰）
    picosleep.deepsleep(0)

    # 復帰後、ボタンが1秒以上押されていたら「長押し」と判定
    btn = machine.Pin(BUTTON_PIN, machine.Pin.IN, machine.Pin.PULL_UP)
    hold_time = 0
    print("Woke up! Checking button hold time...")
    while btn.value() == 0:
        time.sleep(0.1)
        hold_time += 0.1
        if hold_time > 3:
            break
    if hold_time > 1:
        print("Wakeup by long press! ({} sec)".format(round(hold_time,1)))
        break  # 長押しなら終了
    else:
        print("Wakeup by short press! Going back to sleep...")
        # 短押しなら再度スリープ
```

---

最終更新: 2025/07/28
