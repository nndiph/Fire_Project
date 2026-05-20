<?php
date_default_timezone_set('Asia/Jakarta');
header('Content-Type: application/json');
require 'koneksi.php';

$conn->query("SET time_zone = '+07:00'");

// --- Parameter filter & pagination ---
$limit   = min((int)($_GET['limit']  ?? 50), 100);
$offset  = (int)($_GET['offset'] ?? 0);
$tanggal = isset($_GET['tanggal']) ? $conn->real_escape_string($_GET['tanggal']) : '';
$status  = isset($_GET['status']) ? $conn->real_escape_string($_GET['status']) : '';

// --- WHERE: default ambil semua data (termasuk status safe tiap satu jam sekali) ---
$where_clauses = [];

if ($tanggal !== '') {
    $where_clauses[] = "DATE(created_at) = '$tanggal'";
}

if ($status !== '') {
    if ($status === 'CRITICAL') {
        $where_clauses[] = "status_api = 1 AND suhu > 45";
    } elseif ($status === 'WARNING') {
        $where_clauses[] = "status_api = 0 AND suhu > 45";
    } elseif ($status === 'SAFE') {
        $where_clauses[] = "status_api = 0 AND suhu <= 45";
    } elseif ($status === 'DANGER') {
        $where_clauses[] = "status_api = 1 AND suhu <= 45";
    }
}

$where = "";
if (count($where_clauses) > 0) {
    $where = "WHERE " . implode(" AND ", $where_clauses);
}

// --- Query logs dari sensor_logs ---
$query = "SELECT id, node_id, suhu, status_api, gas_level, created_at
          FROM sensor_logs
          $where
          ORDER BY id DESC
          LIMIT $limit OFFSET $offset";
$result = $conn->query($query);

// --- Total baris ---
$total = (int)$conn->query("SELECT COUNT(*) AS total FROM sensor_logs $where")
              ->fetch_assoc()['total'];

// --- Summary stats ---
$s = $conn->query(
    "SELECT
        SUM(CASE WHEN (status_api = 1 OR suhu > 45) THEN 1 ELSE 0 END) AS total_logs,
        SUM(CASE WHEN status_api = 1 AND suhu > 45 THEN 1 ELSE 0 END) AS total_critical,
        SUM(CASE WHEN status_api = 0 AND suhu > 45 THEN 1 ELSE 0 END) AS total_warning,
        SUM(CASE WHEN status_api = 1 AND suhu <= 45 THEN 1 ELSE 0 END) AS total_fire_only
     FROM sensor_logs"
)->fetch_assoc();

$logs = [];
if ($result && $result->num_rows > 0) {
    while ($row = $result->fetch_assoc()) {
        $logs[] = $row;
    }
}

echo json_encode([
    "status"  => "success",
    "total"   => $total,
    "limit"   => $limit,
    "offset"  => $offset,
    "summary" => [
        "total_logs"     => (int)($s['total_logs'] ?? 0),
        "total_critical" => (int)($s['total_critical'] ?? 0),
        "total_warning"  => (int)($s['total_warning'] ?? 0),
        "total_safe"     => (int)($s['total_fire_only'] ?? 0),
    ],
    "logs" => $logs
]);

$conn->close();
?>
