# Run in an elevated or normal PowerShell window
"Vendor:Device`tType"
Get-PnpDevice -PresentOnly |
  Where-Object { $_.InstanceId -like 'PCI\VEN_*' } |
  ForEach-Object {
    $id = $_.InstanceId
    if ($id -match 'VEN_([0-9A-F]{4}).*DEV_([0-9A-F]{4})') {
      $ven = "0x$($matches[1])"
      $dev = "0x$($matches[2])"
      $name = $_.FriendlyName
      if (-not $name) {
        $desc = Get-PnpDeviceProperty -InstanceId $id -KeyName 'DEVPKEY_Device_DeviceDesc' -ErrorAction SilentlyContinue
        $name = if ($desc) { $desc.Data } else { $_.Class }
      }
      "{0}:{1}`t{2}" -f $ven, $dev, $name
    }
  } | Sort-Object
