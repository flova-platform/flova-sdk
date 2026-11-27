# FlovaSDK

[English](README.md) · [مستندات SDK](https://docs.flova.ir/fa/docs/sdk/overview) ·
[آموزش اسکچ](https://docs.flova.ir/fa/docs/sdk/first-sketch/) · [نسخه‌ها](https://github.com/flova-platform/flova-sdk/tags)

FlovaSDK کیت توسعهٔ C++ برای اتصال دستگاه‌های ESP32، ESP8266، Linux و
سخت‌افزارهای سفارشی به فلووا است. SDK دیتاستریم‌های نوع‌دار، راه‌اندازی دستگاه،
پیکربندی و اتصال Flova Link را فراهم می‌کند.

## نصب و انتخاب ابزار

این نمونه‌ها برای سورس `0.5.4` هستند. اگر مدیر کتابخانه این نسخه را ارائه
نمی‌دهد، [راهنمای نصب از سورس](https://docs.flova.ir/fa/docs/sdk/install) را دنبال کنید.

- **Arduino IDE:** بستهٔ برد را در **Boards Manager** و `FlovaSDK` را در
  **Library Manager** نصب کنید. سپس برد و پورت سریال را انتخاب کنید و از
  `#include <FlovaSDK.h>` استفاده کنید. [راهنمای نصب](https://docs.flova.ir/fa/docs/sdk/install)
  مراحل کامپایل، آپلود و مشاهدهٔ خروجی سریال را توضیح می‌دهد.
- **PlatformIO:** وابستگی زیر را به `platformio.ini` اضافه کنید. برای ESP8266
  از [پروفایل آماده](packages/flova-sdk-release/extras/platformio/esp8266/platformio.ini)
  استفاده کنید.
- **Flova IDE:** برای ویرایش، Build و آپلود USB از مرورگر،
  [راهنمای Flova IDE](https://docs.flova.ir/fa/docs/sdk/ide) را ببینید.
- **Linux و Raspberry Pi:** بستهٔ CMake را نصب کنید و برنامه را به
  `Flova::Linux` پیوند دهید. مراحل در [راهنمای نصب](https://docs.flova.ir/fa/docs/sdk/install) آمده است.

```ini
lib_deps = flova-platform/FlovaSDK@^0.5.4
```

## شروع سریع

برای اتصال اولین دستگاه و کنترل LED، [شروع سریع](https://docs.flova.ir/fa/docs/sdk/quickstart)
را دنبال کنید. اگر برنامهٔ Arduino خودتان را دارید، از
[آموزش اسکچ مدیریت‌شده](https://docs.flova.ir/fa/docs/sdk/first-sketch/) شروع کنید: برنامه GPIO را
کنترل می‌کند و SDK راه‌اندازی Wi-Fi و اتصال مجدد را بر عهده دارد.

راهنماهای [دیتاستریم‌ها](https://docs.flova.ir/fa/docs/sdk/datastreams)،
[راه‌اندازی](https://docs.flova.ir/fa/docs/sdk/provisioning)،
[چرخهٔ عمر و OTA](https://docs.flova.ir/fa/docs/sdk/lifecycle) و
[مرجع API](https://docs.flova.ir/fa/docs/sdk/reference) جزئیات را پوشش می‌دهند.

## مجوز

FlovaSDK تحت [مجوز MIT](LICENSE) منتشر می‌شود.
