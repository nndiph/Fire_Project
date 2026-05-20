<?php
date_default_timezone_set('Asia/Jakarta');
header('Content-Type: application/json');
require 'koneksi.php';

// Paksa zona waktu MySQL sesi ini
$conn->query("SET time_zone = '+07:00'");

// Pastikan kolom-kolom penting ada di tb_kontrol (auto-migrate jika belum ada)
$conn->query("ALTER TABLE tb_kontrol ADD COLUMN IF NOT EXISTS last_seen TIMESTAMP NULL DEFAULT NULL");
$conn->query("ALTER TABLE tb_kontrol ADD COLUMN IF NOT EXISTS node2_status TINYINT NOT NULL DEFAULT 0");
$conn->query("ALTER TABLE tb_kontrol ADD COLUMN IF NOT EXISTS mode_system VARCHAR(50) NOT NULL DEFAULT 'manual'");
$conn->query("ALTER TABLE tb_kontrol ADD COLUMN IF NOT EXISTS mode_sistem VARCHAR(50) NOT NULL DEFAULT 'manual'");

// ============================================================
//  BAGIAN 1: Cek ONLINE/OFFLINE dari tb_kontrol.last_seen & node2_status
// ============================================================
$OFFLINE_THRESHOLD = 30; // ESP32 POST tiap 10 detik, toleransi 3x miss

$kontrol = $conn->query(
    "SELECT *, TIMESTAMPDIFF(SECOND, last_seen, NOW()) AS detik_lalu
     FROM tb_kontrol WHERE id = 1"
);

$status_koneksi  = 'OFFLINE';
$detik_lalu      = 999;
$signal_strength = 0;
$node2_status    = 0;
$servo_h         = 90;
$servo_v         = 90;
$mode_system     = 'manual';

if ($kontrol && $kontrol->num_rows > 0) {
    $kRow = $kontrol->fetch_assoc();
    if ($kRow['last_seen'] !== null) {
        $detik_lalu     = (int)$kRow['detik_lalu'];
        $status_koneksi = ($detik_lalu > $OFFLINE_THRESHOLD) ? 'OFFLINE' : 'ONLINE';
        $signal_strength = ($status_koneksi === 'OFFLINE') ? 0 : null;
        $node2_status    = ($status_koneksi === 'OFFLINE') ? 0 : (int)$kRow['node2_status'];
    }
    $servo_h     = (int)($kRow['sudut_servo'] ?? 90);
    $servo_v     = (int)($kRow['sudut_vertikal'] ?? 90);
    $mode_system = $kRow['mode_system'] ?? $kRow['mode_sistem'] ?? 'manual';
}

// ============================================================
//  BAGIAN 2: Data sensor terbaru dari sensor_logs
// ============================================================
$result = $conn->query("SELECT * FROM sensor_logs ORDER BY id DESC LIMIT 1");

if ($result && $result->num_rows > 0) {
    $row = $result->fetch_assoc();

    if ($signal_strength === null) {
        $signal_strength = isset($row['signal_strength']) ? (int)$row['signal_strength'] : 0;
    }

    echo json_encode([
        "status" => "success",
        "data"   => [
            "node_id"          => $row['node_id'] ?? 'NODE1',
            "suhu"             => $row['suhu'],
            "status_api"       => intval($row['status_api']),  // selalu integer
            "water_level"      => $row['gas_level'],           // gas_level dipakai sbg level air
            "signal_strength"  => $signal_strength,
            "pump_status"      => intval($row['pump_status'] ?? 0),
            "solenoid_status"  => intval($row['solenoid_status'] ?? 0),
            "node2_status"     => $node2_status,
            "status_koneksi"   => $status_koneksi,
            "detik_lalu"       => $detik_lalu,
            "timestamp"        => $row['created_at'],
            "waktu_db"         => $row['created_at'],
            "servo_h"          => $servo_h,
            "servo_v"          => $servo_v,
            "mode_system"      => $mode_system
        ]
    ]);
} else {
    // Belum ada data sensor sama sekali
    echo json_encode([
        "status" => "success",
        "data"   => [
            "node_id"          => "NODE1",
            "suhu"             => "--",
            "status_api"       => 0,
            "water_level"      => 0,
            "signal_strength"  => 0,
            "pump_status"      => 0,
            "solenoid_status"  => 0,
            "node2_status"     => $node2_status,
            "status_koneksi"   => $status_koneksi,
            "detik_lalu"       => $detik_lalu,
            "timestamp"        => null,
            "waktu_db"         => null,
            "servo_h"          => $servo_h,
            "servo_v"          => $servo_v,
            "mode_system"      => $mode_system
        ]
    ]);
}

$conn->close();
?>