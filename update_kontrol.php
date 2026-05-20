<?php
header("Content-Type: application/json");
require 'koneksi.php';

// Ensure mode_system and mode_sistem columns exist
$conn->query("ALTER TABLE tb_kontrol ADD COLUMN IF NOT EXISTS mode_system VARCHAR(50) NOT NULL DEFAULT 'manual'");
$conn->query("ALTER TABLE tb_kontrol ADD COLUMN IF NOT EXISTS mode_sistem VARCHAR(50) NOT NULL DEFAULT 'manual'");

$data = json_decode(file_get_contents("php://input"), true);
if (!$data) {
    // Return current state if no POST data
    $res = $conn->query("SELECT mode_system, mode_sistem, sudut_servo, sudut_vertikal, pwm_pompa, status_pompa, status_solenoid FROM tb_kontrol WHERE id = 1");
    $row = $res->fetch_assoc();
    if ($row) {
        $row['mode_system'] = $row['mode_system'] ?? $row['mode_sistem'] ?? 'manual';
        $row['mode_sistem'] = $row['mode_system'];
    }
    echo json_encode(["status" => "success", "data" => $row]);
    exit;
}

// Fetch current values
$res = $conn->query("SELECT mode_system, mode_sistem, sudut_servo, sudut_vertikal, pwm_pompa, status_pompa, status_solenoid FROM tb_kontrol WHERE id = 1");
$row = $res->fetch_assoc();

$mode = $row['mode_system'] ?? $row['mode_sistem'] ?? 'manual';
$h = (int)$row['sudut_servo'];
$v = (int)$row['sudut_vertikal'];
$pwm = (int)$row['pwm_pompa'];
$pump = (int)$row['status_pompa'];
$solenoid = (int)$row['status_solenoid'];

$updated = false;

// Handle Mode Toggle
if (isset($data['mode_system'])) {
    $mode = $data['mode_system'] === 'auto' ? 'auto' : 'manual';
    $updated = true;
}

// Handle Directional Move (also sets mode to manual)
if (isset($data['axis']) && isset($data['move'])) {
    $step = 5;
    if ($data['axis'] === 'h') {
        if ($data['move'] === 'left') $h -= $step;
        if ($data['move'] === 'right') $h += $step;
        $h = max(0, min(180, $h));
    } else if ($data['axis'] === 'v') {
        if ($data['move'] === 'down') $v -= $step;
        if ($data['move'] === 'up') $v += $step;
        $v = max(0, min(180, $v));
    }
    $mode = 'manual'; // Auto switch to manual when moving
    $updated = true;
}

if (isset($data['reset'])) {
    $h = 90;
    $v = 90;
    $mode = 'manual';
    $updated = true;
}

if (isset($data['pwm'])) {
    $pwm = max(0, min(255, (int)$data['pwm']));
    $mode = 'manual';
    $updated = true;
}

if (isset($data['pump'])) {
    $pump = $data['pump'] ? 1 : 0;
    $mode = 'manual';
    $updated = true;
}

if (isset($data['solenoid'])) {
    $solenoid = $data['solenoid'] ? 1 : 0;
    $mode = 'manual';
    $updated = true;
}

if ($updated) {
    $stmt = $conn->prepare("UPDATE tb_kontrol SET mode_system=?, mode_sistem=?, sudut_servo=?, sudut_vertikal=?, pwm_pompa=?, status_pompa=?, status_solenoid=? WHERE id = 1");
    $stmt->bind_param("ssiiiii", $mode, $mode, $h, $v, $pwm, $pump, $solenoid);
    $stmt->execute();
}

echo json_encode([
    "status" => "success",
    "data" => [
        "mode_system" => $mode,
        "mode_sistem" => $mode,
        "sudut_servo" => $h,
        "sudut_vertikal" => $v,
        "pwm_pompa" => $pwm,
        "status_pompa" => $pump,
        "status_solenoid" => $solenoid
    ]
]);

$conn->close();
?>
