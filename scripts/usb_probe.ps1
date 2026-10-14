param([int]$ForwardedPort = 49322)
# Requires a separately installed/validated libusbmuxd iproxy process forwarding
# this localhost port to an iOS USBProbe listener. This script does not create USB.
$ErrorActionPreference = 'Stop'
$client = [System.Net.Sockets.TcpClient]::new()
try {
  $client.Connect('127.0.0.1',$ForwardedPort)
  $stream = $client.GetStream(); $stream.ReadTimeout = 3000; $stream.WriteTimeout = 3000
  $nonce = [System.Text.Encoding]::UTF8.GetBytes("LiveLink-USB-" + [guid]::NewGuid().ToString())
  $stream.Write($nonce,0,$nonce.Length)
  $response = [byte[]]::new($nonce.Length); $offset = 0
  while ($offset -lt $response.Length) { $count = $stream.Read($response,$offset,$response.Length-$offset); if ($count -eq 0) { throw 'Peer closed before completing echo' }; $offset += $count }
  if ([Convert]::ToBase64String($response) -ne [Convert]::ToBase64String($nonce)) { throw 'Echo mismatch' }
  Write-Output 'PASS bidirectional TCP echo through the supplied forwarder. Record hardware, driver versions and route before calling this verified USB.'
} finally { $client.Dispose() }
