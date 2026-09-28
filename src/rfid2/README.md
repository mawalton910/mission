Port A RFID2 driver

This is the public-domain MFRC522 I2C implementation shipped with M5Stack M5Dial:
https://github.com/m5stack/M5Dial/tree/master/src/utility

The class was renamed to PortARfid2 to coexist with the built-in reader. Its four
register IO operations use M5.Ex_I2C instead of M5.In_I2C. The RFID protocol and
UID cascade handling are unchanged. This keeps the external reader self-contained
without requiring users to modify their installed M5Dial library.

Unit RFID2 uses WS1850S at I2C address 0x28. This is different from Unit NFC
(ST25R3916 at 0x50). Both are described separately in M5Stack documentation.
