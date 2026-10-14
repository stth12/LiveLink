Set shell = CreateObject("WScript.Shell")
Set fs = CreateObject("Scripting.FileSystemObject")
folder = fs.GetParentFolderName(WScript.ScriptFullName)
shell.Run Chr(34) & folder & "\Live Link Service.exe" & Chr(34), 0, False
