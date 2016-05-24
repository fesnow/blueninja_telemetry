# BlueNinja Telemetry
BlueNinja向けのテレメトリープログラムです。  

## 機能
* BluetoothLE(BLE)で9軸モーションセンサーの計測データを送信
  * 角速度3軸(-2000 digrees/s ～ 2000 digrees/s)
  * 加速度3軸(-16G ～ 16G)
  * 地磁気3軸(センサ生値 -32768 ～ 32767)
  * 送信間隔設定(100ms ～ 1000ms)
* BLEデバイス名(LocalName)の設定
  * アプリケーションでLocalNameによる複数台の識別が可能  
  (BLE Addressはランダムアドレスなので識別にアドレスを使用できません)

## 動作したBLEセントラルデバイス
* iOS  
iPod touch 5th gen iOS 9.3.1 + BLExplrにて確認
* Android 5.0以降  
Nexus6 Android 6.0.1 + サンプルアプリにて確認
* BlueZ + node.js + noble  
Raspberry Piで確認

## BLEプロファイル
以下のGATTサービスを含みます。  

|サービス名                |UUID                                |
|--------------------------|------------------------------------|
|Config Service            |00010000-4346-4c6d-af25-e5b125f0dfe3|
|Motion sensor Service     |00020000-4346-4c6d-af25-e5b125f0dfe3|

Advertisingパケットには、Complete local name(AD Type: 0x09)にLocalNameの設定値を、
Advertising Responsパケットには、Complete list of 128-bit UUIDs available(AD Type: 0x07)に00010000-4346-4c6d-af25-e5b125f0dfe3を設定して送信します。  
複数台を同時に動作させる際の識別に使用してください。

### BNTelemetry Config Service
|キャラクタリスティック|UUID                                |
|----------------------|------------------------------------|
|name                  |00010001-4346-4c6d-af25-e5b125f0dfe3|

#### name
BLEのLocalNameを設定/参照します。  
最大で22Byteまでの文字列を設定できます。

### BNTelemetry MotionSensor Service
|キャラクタリスティック|UUID                                |
|----------------------|------------------------------------|
|interval              |00020001-4346-4c6d-af25-e5b125f0dfe3|
|value                 |00020002-4346-4c6d-af25-e5b125f0dfe3|

#### interval
計測結果の送信間隔を設定/参照します。  
設定値は1～10、単位は100msです。100ms～1000msの範囲で設定できます。

#### value
計測値を通知(Notification)します。  
一回のNotificationで下記のフォーマットの18Byteを送信します。
<table>
    <tr>
        <th>Byte array index</th>
        <th>00</th><th>01</th><th>02</th><th>03</th><th>04</th><th>05</th>
    </tr>
    <tr>
        <th rowspan="3">Value</th>
        <th colspan="6">Gyro</th>
    </tr>
    <tr>
        <td colspan="2">X</td>
        <td colspan="2">Y</td>
        <td colspan="2">Z</td>
    </tr>
    <tr>
        <td>LSB</td><td>MSB</td>
        <td>LSB</td><td>MSB</td>
        <td>LSB</td><td>MSB</td>
    </tr>
    <tr>
        <th>Byte array index</th>
        <th>06</th><th>07</th><th>08</th><th>09</th><th>10</th><th>11</th>
    </tr>
    <tr>
        <th rowspan="3">Value</th>
        <th colspan="6">Acceleromter</th>
    </tr>
    <tr>
        <td colspan="2">X</td>
        <td colspan="2">Y</td>
        <td colspan="2">Z</td>
    </tr>
    <tr>
        <td>LSB</td><td>MSB</td>
        <td>LSB</td><td>MSB</td>
        <td>LSB</td><td>MSB</td>
    </tr>
    <tr>
        <th>Byte array index</th>
        <th>12</th><th>13</th><th>14</th><th>15</th><th>16</th><th>17</th>
    </tr>
    <tr>
        <th rowspan="3">Value</th>
        <th colspan="6">Magnetometer</th>
    </tr>
    <tr>
        <td colspan="2">X</td>
        <td colspan="2">Y</td>
        <td colspan="2">Z</td>
    </tr>
    <tr>
        <td>LSB</td><td>MSB</td>
        <td>LSB</td><td>MSB</td>
        <td>LSB</td><td>MSB</td>
    </tr>
</table>

計測値はMPU-9250のRAWデータです。  
角速度は、-2000 degree/s ～ 2000 degree/sで1/16.4(≒0.06)degree/s単位です。  
加速度は、-16G ～ 6Gで1/2048(≒0.00005)G単位です。  
地磁気は、物理量への変換せず大小比較にご使用ください。
