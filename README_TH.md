# เมืองปทุมธานี V2 · ดูกล้อง 2 จุดโดยไม่เปิดโปรแกรมบนคอม

เวอร์ชันนี้แยกจาก V1 เดิม ใช้ **GitHub Actions** เป็นเครื่องสร้างภาพนิ่ง JPEG จากสตรีมวิดีโอสาธารณะทั้งสองแห่ง และใช้ **GitHub Pages** ส่งภาพให้ ESP32-CYD ทาง Wi-Fi ผู้ใช้เปิดบอร์ดและต่อ Wi-Fi ได้โดยไม่ต้องเปิด Python/FFmpeg บนคอมที่บ้าน หลังตั้งค่า GitHub ครั้งแรก ระบบบน GitHub ทำงานตามรอบเอง

ซอร์สใหม่แจกภายใต้ `LICENSE` (MIT); ฟอนต์ `sarabun14_fixed.h` ที่คัดจากโปรเจกต์เดิมสร้างจาก IBM Plex Sans Thai ซึ่งมี [SIL Open Font License 1.1](https://github.com/IBM/plex/blob/master/LICENSE.txt) แยกต่างหาก

**นี่คือภาพนิ่งเป็นช่วง ๆ ไม่ใช่ CCTV สด**: GitHub พยายามจับภาพทุกประมาณ 5 นาที งานอาจเริ่มช้า หรือ Pages อาจเผยแพร่ช้า จอและเว็บแสดงเวลาจริงของภาพ และปฏิเสธภาพที่เก่ากว่า **15 นาที** ใช้ประกอบการดูน้ำเท่านั้น ไม่ใช้แทนประกาศเตือนภัยของหน่วยงาน

## 1. ตั้ง GitHub ครั้งเดียว

1. แตก ZIP ให้ได้โฟลเดอร์ `MUANG_PATHUMTHANI_CLOUD_NO_PC_V2` และดูว่ามี `.github/workflows/water-camera-pages.yml` อยู่ (Windows Explorer อาจซ่อนโฟลเดอร์ขึ้นต้นด้วยจุด)
2. เข้าสู่ GitHub สร้าง repository **Public** ใหม่ เช่น `pathum-water` นำ **เนื้อหาในโฟลเดอร์** นี้ขึ้นที่รากของ repository: ต้องมี `CLOUD_FRAME_CAPTURE.py`, `docs/index.html`, `.github/workflows/water-camera-pages.yml` โดยอยู่ในตำแหน่งตรงนี้ ไม่ใช่ซ้อนในโฟลเดอร์อีกชั้น
3. ไปที่ **Settings → Pages → Build and deployment → Source → GitHub Actions**; จากนั้นเปิดแท็บ **Actions → Update Pathum Thani camera stills → Run workflow** เพื่อทดสอบทันที และเปิด log ตรวจว่า `camera 1` / `camera 2` เขียน JPEG ได้
4. เปิด `https://<ชื่อบัญชี>.github.io/<ชื่อ-repository>/` ดูรูปและเวลา ถ้าขึ้นว่าไม่มีภาพใหม่ ให้เปิด log ใน Actions: อาจเป็นเพราะ GitHub เข้าถึงกล้องต้นทางไม่ได้, งานยังไม่ได้รัน หรือ Pages ยังไม่เผยแพร่; สตรีม HLS 200 บนเน็ตของผู้ใช้ไม่ได้ยืนยันว่า GitHub runner อ่าน segment วิดีโอได้

ไฟล์งาน `.github/workflows/water-camera-pages.yml` ทำงานตามรอบ 5 นาที และจัดหน้าเว็บกับภาพ JPEG 320×128 พิกเซล ภาพของแต่ละกล้องมี 4 ระดับซูม ครอปจากภาพต้นทางบน GitHub หากจับภาพแหล่งใดไม่สำเร็จ จะส่ง timestamp `0` และ **ไม่ส่งภาพเก่าค้าง**

## 2. ติดตั้ง ESP32-CYD ที่เคยใช้

1. เปิด `MUANG_PATHUMTHANI_CLOUD_NO_PC_V2.ino` ด้วย Arduino IDE 1.8.19 ติดตั้งบอร์ด **ESP32 Dev Module** และ LovyanGFX เหมือนโปรเจกต์ ST7789 + XPT2046 เดิม ตรวจพอร์ต COM ให้ถูก แล้วกด Verify / Upload โปรเจกต์นี้จัดแนวนอน USB อยู่ขวา **ถ้าจอเป็น ILI9341 ต้องแก้ `cyd_display.h` ตามบอร์ดก่อน**
2. Wi-Fi ที่เคยบันทึกจาก V1 จะถูกนำมาใช้ (`Preferences` namespace เดิม) เมื่อไม่มีลิงก์ GitHub Pages บอร์ดจะเปิด Wi-Fi ตั้งค่า `PATHUM-XXXXXXXX` พร้อมรหัสเฉพาะบอร์ดบนจอ
3. มือถือต่อ Wi-Fi ของบอร์ด เปิด `http://192.168.4.1` กรอก Wi-Fi 2.4 GHz กับลิงก์ Pages ในข้อ 1 เช่น `https://myname.github.io/pathum-water` (ไม่มี `/` ท้าย) กดบันทึก บอร์ดรีสตาร์ตและจำการตั้งค่าไว้
4. รอให้ GitHub Actions เผยแพร่ภาพ กด **สะพานแดง** หรือ **ปทุม** เพื่อสลับกล้อง, **+ / −** ซูม 1–4 เท่า จอแสดงเวลาที่จับภาพและอายุของภาพ กดค้างวันที่/เวลา 3.5 วินาทีเพื่อแก้ Wi-Fi หรือ URL

Serial Monitor **115200 baud** จะรายงาน `[WIFI]`, `[CLOUD CAM 1/2] meta HTTP`, `JPEG HTTP/length`, `valid/drawn` และ `stale/invalid time` ถ้าภาพไม่มา ส่ง log เหล่านี้พร้อม log จาก GitHub Actions มาตรวจได้ (ไม่ต้องส่งรหัส Wi-Fi)

## 3. แจกไฟล์ติดตั้งผ่านหน้า GitHub หลังทดสอบบอร์ด

ESP32 ใช้ **`.bin`** ไม่ใช่ `.hex` ตามปกติ ยังไม่มี `merged.bin` ในชุดนี้ เพราะไม่มีบอร์ดจริง/Arduino toolchain ในเครื่องที่สร้างโครงการนี้ หน้า Pages ซ่อนปุ่มแฟลชจนกว่าจะใส่ไฟล์จริงที่ผ่านทดสอบแล้ว

ใน Arduino IDE เลือก **Sketch → Export compiled Binary** แล้วนำ bootloader, partitions, boot_app0, application ที่ตรงกับบอร์ดให้ `MAKE_FACTORY_BIN.py` ตามคำสั่งนี้ (แทนพาธด้วยไฟล์จริง):

```bat
python -m pip install esptool
python MAKE_FACTORY_BIN.py --bootloader "C:\path\sketch.ino.bootloader.bin" --partitions "C:\path\sketch.ino.partitions.bin" --boot-app0 "C:\path\boot_app0.bin" --application "C:\path\sketch.ino.bin"
```

จะได้ `docs/firmware/merged.bin` อัปโหลดไฟล์นั้นเข้าที่เดิมใน repository แล้วแฟลชผ่านหน้า Pages ทดสอบบนบอร์ด **ST7789 รุ่นเดียวกัน** ก่อนแจกให้ผู้อื่น ห้ามอัปโหลดสำเนา NVS หรือไฟล์สำรองจากบอร์ดที่กรอกรหัส Wi-Fi แล้ว

## ทดสอบได้แค่ไหน

- `python CLOUD_FRAME_CAPTURE.py --output test_frames --self-test` สร้างภาพจำลอง 2 กล้อง × 4 ซูม ทดสอบการทำภาพและ timestamp บนเครื่องสร้างชุดนี้แล้ว
- ตรวจภาพจริงจากเบราว์เซอร์พบ snapshot ของสะพานแดงเป็น JPEG 1920×1080 ที่อัปเดตราวทุก 2 นาที; **รุ่น GitHub นี้ใช้ HLS ของสะพานแดงและปทุมแทน** เพื่อให้แสดงทั้งสองกล้องผ่านระบบเดียวกัน
- ยังไม่ได้รัน workflow บนบัญชี GitHub ของคุณหรือแฟลช CYD จริง จึงยังไม่ยืนยันว่ารูปจริงทั้งสองจุดผ่าน GitHub runner และแสดงบนบอร์ดได้ โปรดตรวจ Actions และ Serial ก่อนเผยแพร่
- การดาวน์โหลดจาก GitHub Pages ในตัวอย่างเฟิร์มแวร์ใช้ `WiFiClientSecure.setInsecure()` กับไฟล์ภาพสาธารณะเท่านั้น จึงไม่ส่งรหัสผ่านไปที่ Pages แต่ยัง **ไม่ได้ตรวจใบรับรอง TLS** โปรดอย่าใช้ภาพนี้ตัดสินเหตุฉุกเฉินเพียงอย่างเดียว

## ถ้า V1 บน Windows เปิด 1 วินาทีแล้วดับ

ต้อง **แตก ZIP ลงโฟลเดอร์ก่อน** แล้วเปิด `START_WINDOWS.bat` จากโฟลเดอร์ที่แตกจริง ไม่เปิดไฟล์ `.bat` จากหน้าต่างดูไฟล์ภายใน ZIP; ถ้ายังดับ เปิด Command Prompt ในโฟลเดอร์นั้นแล้วรัน `START_WINDOWS.bat` เพื่อเห็นข้อความผิดพลาด เวอร์ชัน V2 นี้ไม่ใช้ไฟล์ Windows ดังกล่าว
