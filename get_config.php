<?php
header('Content-Type: application/json');
require 'koneksi.php';

$res = $conn->query("SELECT mode_system, mode_sistem, sudut_servo, sudut_vertikal, status_pompa FROM tb_kontrol WHERE id = 1");
if ($res && $res->num_rows > 0) {
    $row = $res->fetch_assoc();
    $mode = $row['mode_system'] ?? $row['mode_sistem'] ?? 'manual';
    echo json_encode([
        "mode" => $mode,
        "h" => (int)$row['sudut_servo'],
        "v" => (int)$row['sudut_vertikal'],
        "pump" => (int)$row['status_pompa']
    ]);
} else {
    echo json_encode(["mode" => "auto", "h" => 90, "v" => 90, "pump" => 0]);
}
?>
