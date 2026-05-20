<?php
$host = "localhost";
$user = "root";
$pass = ""; // Cek jika XAMPP kamu ada passwordnya
$db = "db_fire_system"; // Pastikan nama database ini sudah kamu buat di phpMyAdmin

$conn = new mysqli($host, $user, $pass, $db);

if ($conn->connect_error) {
    die("Koneksi gagal: " . $conn->connect_error);
}
?>