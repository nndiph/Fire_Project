<?php
date_default_timezone_set('Asia/Jakarta');
header("Content-Type: application/json");
require 'koneksi.php';

// ============================================================
//  BACA INPUT: Support dua format sekaligus
//  1. application/json       (ESP32 firmware baru - JSON body)
//  2. application/x-www-form-urlencoded (test_api.http / form)
// ============================================================
$node_id         = 'NODE1';
$suhu            = 0;
$gas_level       = 0;
$status_api      = 0;
$signal          = 0;
$pump_status     = 0;
$solenoid_status = 0;
$node2_status    = 0;

$contentType = $_SERVER['CONTENT_TYPE'] ?? '';

if (strpos($contentType, 'application/json') !== false) {
    // --- Baca dari JSON body ---
    $body = file_get_contents("php://input");
    $json = json_decode($body, true);

    if (!$json) {
        echo json_encode(["status" => "error", "msg" => "JSON tidak valid"]);
        exit;
    }

    $node_id         = isset($json['node_id'])         ? $conn->real_escape_string($json['node_id']) : 'NODE1';
    $suhu            = isset($json['suhu'])             ? floatval($json['suhu'])            : 0;
    $gas_level       = isset($json['water_level'])      ? floatval($json['water_level'])     : 0; // field 'water_level' dari ESP32
    $status_api      = isset($json['status_api'])       ? intval($json['status_api'])        : 0;
    $signal          = isset($json['signal_strength'])   ? intval($json['signal_strength'])   : 0;
    $pump_status     = isset($json['pump_status'])       ? intval($json['pump_status'])       : 0;
    $solenoid_status = isset($json['solenoid_status'])   ? intval($json['solenoid_status'])   : 0;
    $node2_status    = isset($json['node2_status'])      ? intval($json['node2_status'])      : 0;

} else {
    // --- Baca dari $_POST (form-urlencoded) ---
    $node_id         = isset($_POST['node_id'])          ? $conn->real_escape_string($_POST['node_id']) : 'NODE1';
    $suhu            = isset($_POST['suhu'])              ? floatval($_POST['suhu'])           : 0;
    $gas_level       = isset($_POST['gas_level'])         ? floatval($_POST['gas_level'])      : 0;
    $status_api      = isset($_POST['status_api'])        ? intval($_POST['status_api'])       : 0;
    $signal          = isset($_POST['signal_strength'])   ? intval($_POST['signal_strength'])  : 0;
    $pump_status     = isset($_POST['pump_status'])       ? intval($_POST['pump_status'])      : 0;
    $solenoid_status = isset($_POST['solenoid_status'])   ? intval($_POST['solenoid_status'])  : 0;
    $node2_status    = isset($_POST['node2_status'])     ? intval($_POST['node2_status'])     : 0;
}


// Pastikan kolom node2_status ada di tb_kontrol (auto-migrate jika belum ada)
$conn->query("ALTER TABLE tb_kontrol ADD COLUMN IF NOT EXISTS node2_status TINYINT NOT NULL DEFAULT 0");

// ── HEARTBEAT: Selalu update last_seen dan status Node 2, tanpa syarat ─────────
$conn->query("UPDATE tb_kontrol SET last_seen = NOW(), node2_status = $node2_status WHERE id = 1");

// ── Cek kondisi terakhir di database ─────────────────────────
$lastRow    = $conn->query("SELECT status_api, created_at FROM sensor_logs ORDER BY id DESC LIMIT 1");
$lastStatus = -1;
$lastTime   = null;

if ($lastRow && $lastRow->num_rows > 0) {
    $row        = $lastRow->fetch_assoc();
    $lastStatus = intval($row['status_api']);
    $lastTime   = $row['created_at'];
}

// ═══════════════════════════════════════════════════════════════
//  KONDISI 1 — PERUBAHAN STATUS (0 -> 1 atau 1 -> 0)
// ═══════════════════════════════════════════════════════════════
if ($status_api != $lastStatus) {
    $conn->query(
        "INSERT INTO sensor_logs (node_id, suhu, gas_level, status_api, signal_strength, pump_status, solenoid_status, node2_status)
         VALUES ('$node_id', '$suhu', '$gas_level', '$status_api', '$signal', '$pump_status', '$solenoid_status', '$node2_status')"
    );
    $msg = ($status_api == 1) ? "DANGER SAVED - ALARM TRIGGERED" : "RECOVERY SAVED - ALARM CLEARED";
    echo json_encode(["status" => "success", "msg" => $msg]);
    exit;
}

// ═══════════════════════════════════════════════════════════════
//  KONDISI 2 — STATUS TIDAK BERUBAH (SAMA DENGAN SEBELUMNYA)
// ═══════════════════════════════════════════════════════════════
if ($status_api == 1) {
    // Sedang dalam kondisi bahaya, ESP32 mengirim heartbeat bahaya
    echo json_encode(["status" => "success", "msg" => "DANGER ONGOING, HEARTBEAT ONLY - no duplicate log"]);
    exit;
}

// Jika status == 0 (Aman dan stabil)
if ($lastStatus == -1) {
    // Tabel masih kosong
    $conn->query(
        "INSERT INTO sensor_logs (node_id, suhu, gas_level, status_api, signal_strength, pump_status, solenoid_status, node2_status)
         VALUES ('$node_id', '$suhu', '$gas_level', '$status_api', '$signal', '$pump_status', '$solenoid_status', '$node2_status')"
    );
    echo json_encode(["status" => "success", "msg" => "SAFE SAVED (first record)"]);
    exit;
}

$selisih = time() - strtotime($lastTime);
if ($selisih >= 3600) {
    // Tetap simpan 1 jam sekali agar kita punya history bahwa sistem aktif dan aman
    $conn->query(
        "INSERT INTO sensor_logs (node_id, suhu, gas_level, status_api, signal_strength, pump_status, solenoid_status, node2_status)
         VALUES ('$node_id', '$suhu', '$gas_level', '$status_api', '$signal', '$pump_status', '$solenoid_status', '$node2_status')"
    );
    echo json_encode(["status" => "success", "msg" => "SAFE SAVED (hourly log)"]);
} else {
    $sisaMenit = ceil((3600 - $selisih) / 60);
    echo json_encode(["status" => "success", "msg" => "SAFE IGNORED, HEARTBEAT ONLY - next log in {$sisaMenit} min"]);
}

$conn->close();
?>