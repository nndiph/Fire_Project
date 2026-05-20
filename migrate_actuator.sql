-- ============================================================
--  PyroGuard System - Database Migration
--  Menambahkan kolom pump_status dan solenoid_status
--  ke tabel sensor_logs untuk menyimpan status aktuator.
--
--  Jalankan script ini di phpMyAdmin atau MySQL CLI:
--  mysql -u root db_fire_system < migrate_actuator.sql
-- ============================================================

ALTER TABLE sensor_logs 
ADD COLUMN IF NOT EXISTS pump_status TINYINT NOT NULL DEFAULT 0 AFTER signal_strength,
ADD COLUMN IF NOT EXISTS solenoid_status TINYINT NOT NULL DEFAULT 0 AFTER pump_status;

-- Verifikasi
SELECT COLUMN_NAME, DATA_TYPE, COLUMN_DEFAULT 
FROM INFORMATION_SCHEMA.COLUMNS 
WHERE TABLE_SCHEMA = 'db_fire_system' 
AND TABLE_NAME = 'sensor_logs'
ORDER BY ORDINAL_POSITION;
