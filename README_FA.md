<div dir="rtl" align="center">
  <h1>FlovaSDK</h1>
  <p><strong>ساخت دستگاه‌های متصل مبتنی بر ESP32 و ESP8266 با پلتفرم فلووا</strong></p>
  <p>
    <a href="README.md">English</a> ·
    <a href="https://docs.flova.ir">مستندات</a> ·
    <a href="examples">نمونه‌ها</a> ·
    <a href="https://github.com/flova-platform/flova-sdk/tags">نسخه‌های SDK</a>
  </p>
  <p>
    <a href="https://github.com/flova-platform/flova-sdk/tags"><img alt="نسخه SDK" src="https://img.shields.io/github/v/tag/flova-platform/flova-sdk?filter=v*&amp;sort=semver&amp;style=flat-square&amp;label=SDK"></a>
    <a href="https://registry.platformio.org/libraries/flova-platform/FlovaSDK"><img alt="PlatformIO Registry" src="https://badges.registry.platformio.org/packages/flova-platform/library/FlovaSDK.svg"></a>
    <a href="https://github.com/arduino/library-registry/pull/9043"><img alt="Arduino Library Manager" src="https://img.shields.io/badge/Arduino%20Library%20Manager-FlovaSDK-00878F?style=flat-square&amp;logo=arduino&amp;logoColor=white"></a>
    <a href="LICENSE"><img alt="MIT License" src="https://img.shields.io/github/license/flova-platform/flova-sdk?style=flat-square"></a>
  </p>
</div>

FlovaSDK کیت توسعه رسمی C++ برای اتصال دستگاه‌ها به پلتفرم فلووا است. این SDK
برای ESP32 و ESP8266 یکپارچه‌سازی آماده دارد و برای سخت‌افزارهای سفارشی نیز یک
هسته قابل‌حمل C++11 ارائه می‌کند. در برنامه‌های سفارشی، Flova Link به‌صورت
پیش‌فرض فعال است و می‌توانید `PubSubClient` خودتان را به‌عنوان انتقال MQTT به
همان facade وصل کنید.

> راهنمای کامل راه‌اندازی، دیتاستریم‌ها، پیکربندی دستگاه، OTA و API در
> **[docs.flova.ir](https://docs.flova.ir)** در دسترس است.

## نصب

### PlatformIO

در پروژه ESP32 در PlatformIO، این خط را اضافه کنید:

```ini
  lib_deps = flova-platform/FlovaSDK@^0.4.0
```

برای ESP8266 از فایل آماده‌ی `extras/platformio/esp8266/platformio.ini`
استفاده کنید؛ این فایل چیدمان حافظه‌ی لازم برای heap در IRAM را فعال می‌کند:

```ini
  lib_deps = flova-platform/FlovaSDK@^0.4.0
build_flags =
```

### Arduino IDE

نسخه 0.4.0 از ESP32 و ESP8266 در Arduino IDE پشتیبانی می‌کند:

1. از منوی **Tools → Manage Libraries** وارد مدیریت کتابخانه‌ها شوید.
2. عبارت **FlovaSDK** را جست‌وجو کنید.
3. گزینه **Install** را انتخاب کنید.

سپس هدر واحد SDK را به برنامه اضافه کنید:

```cpp
#include <FlovaSDK.h>
```

این هدر بر اساس برد انتخاب‌شده، `FlovaEsp32` یا `FlovaEsp8266` را در اختیار
برنامه می‌گذارد و helperهای مشترک `FLOVA_*` را نیز شامل می‌شود. برای ESP8266
گزینه **Tools → MMU → 16KB cache + 48KB IRAM and 2nd Heap (shared)** را پیش
از کامپایل انتخاب کنید.

## شروع سریع

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <FlovaSDK.h>

FlovaEsp32 device;
auto relay = FLOVA_DATASTREAM(device, bool, "relay");

void setRelay(bool enabled) {
  digitalWrite(2, enabled ? HIGH : LOW);
}

void setup() {
  WiFi.begin("your-wifi", "your-password");
  pinMode(2, OUTPUT);
  FLOVA_ON_WRITE(relay, setRelay);
  device.begin();
}

void loop() { device.run(); }
```

همه helperهای `FLOVA_*` با هر دو انتقال Flova Link و MQTT کار می‌کنند. کلید
هر datastream باید با template دستگاه یکسان باشد.

## زمان‌بندی ابری و تایمر محلی

زمان‌بندی‌های ساخته‌شده در اپ روی Engine اجرا می‌شوند. دستگاه باید آنلاین باشد؛
Engine در زمان مقرر یک فرمان معمولی می‌فرستد. فرمان پس از ۶۰ ثانیه منقضی می‌شود
و اجراهای ازدست‌رفته پس از اتصال دوباره تکرار نمی‌شوند. فریم‌ور سفارشی باید پیش
از `begin()` اجازهٔ زمان‌بندی ابری را برای هر جریان داده اعلام کند:

```cpp
relay.scheduling(flova::Scheduling::CloudOnly);
```

پیش‌فرض `Scheduling::Disabled` است. کنسول نمی‌تواند این اجازه را فعال کند.
فریم‌ور یونیورسال برای خروجی‌های دیجیتال و PWM این اجازه را به‌صورت صریح می‌دهد.
هیچ برنامهٔ زمان‌بندی روی دستگاه دانلود یا در حافظهٔ دائمی ذخیره نمی‌شود.

برای تایمرهای تعریف‌شده در کد می‌توانید از `FlovaScheduler.h` و
`flova::Scheduler` استفاده کنید. این ابزار حداکثر هشت تایمر را در RAM نگه می‌دارد؛
برنامهٔ شما ساعت معتبر و تبدیل منطقهٔ زمانی را فراهم می‌کند و `run()` را در حلقه
فراخوانی می‌کند. تایمر به شبکه وابسته نیست و پس از راه‌اندازی دوباره باید در
`setup()` ثبت شود. برای حفظ بررسی‌های ایمنی، callback باید از `write()` جریان
داده استفاده کند. این تایمرها مستقل از زمان‌بندی‌های ساخته‌شده در اپ هستند.

## انتقال MQTT

برای MQTT فقط `PubSubClient` خودتان را به سازنده دستگاه بدهید:

```cpp
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <FlovaSDK.h>

WiFiClientSecure socket;
PubSubClient mqtt(socket);
FlovaEsp32 device(mqtt);
```

آدرس MQTT برابر `mqtt.flova.ir:8883` است. شناسه دستگاه username و secret
دستگاه password است و همان datastreamهای Flova Link را پشتیبانی می‌کند. نمونه کامل را در
[`examples/custom-mqtt`](examples/custom-mqtt) ببینید.

برای راه‌اندازی و استفاده در محیط عملیاتی، [نمونه‌ها](examples) را ببینید یا
[مستندات فلووا](https://docs.flova.ir) را دنبال کنید.

## firmware یونیورسال از SDK

firmware یونیورسال را در پروژه خودتان build و upload کنید. ابزار انتخاب‌شده
تصویر کامل برد، شامل bootloader و جدول پارتیشن، را می‌سازد و نیازی به دریافت
فایل firmware آماده نیست.

برای ESP32، `FlovaSDK` را از Arduino Library Manager یا رجیستری PlatformIO
نصب کنید و از `<FlovaUniversalEsp32.h>` استفاده کنید:

```cpp
#include <Arduino.h>
#include <FlovaUniversalEsp32.h>

// اختیاری: نبود رمز، شبکهٔ راه‌اندازی را باز نگه می‌دارد.
FlovaUniversalEsp32 device(FLOVA_SETUP_PASSWORD);

void setup() {
  Serial.begin(115200);
  device.begin();
}

void loop() { device.run(); }
```

از یک برد ESP32 استفاده کنید. Arduino IDE یا PlatformIO تصویر کامل را upload
می‌کند.

برای ESP8266، از هدر `<FlovaUniversalEsp8266.h>` در Arduino IDE یا PlatformIO استفاده کنید.
جزئیات زمان اتصال و حافظه موردنیاز در [راهنمای انتقال داده](packages/TRANSPORT.md) آمده است.

## پیوندها

- [مستندات](https://docs.flova.ir)
- [رجیستری PlatformIO](https://registry.platformio.org/libraries/flova-platform/FlovaSDK)
- [نمونه‌ها](examples)
- [نسخه‌های SDK](https://github.com/flova-platform/flova-sdk/tags)
- [گزارش مشکل](https://github.com/flova-platform/flova-sdk/issues)

## مجوز

FlovaSDK تحت [مجوز MIT](LICENSE) منتشر شده است.
