<?php

header("Content-Type: text/plain");

echo "CGI TYPE: PHP\n";
echo "STATUS: OK\n\n";

echo "REQUEST_METHOD=" . ($_SERVER["REQUEST_METHOD"] ?? "") . "\n";
echo "QUERY_STRING=" . ($_SERVER["QUERY_STRING"] ?? "") . "\n";
echo "SCRIPT_FILENAME=" . ($_SERVER["SCRIPT_FILENAME"] ?? "") . "\n";
echo "SCRIPT_NAME=" . ($_SERVER["SCRIPT_NAME"] ?? "") . "\n";
echo "CONTENT_TYPE=" . ($_SERVER["CONTENT_TYPE"] ?? "") . "\n";
echo "CONTENT_LENGTH=" . ($_SERVER["CONTENT_LENGTH"] ?? "") . "\n";

$body = file_get_contents("php://stdin");

echo "\nBODY:\n";
echo $body;
echo "\n";
?>