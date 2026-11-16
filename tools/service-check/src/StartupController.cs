// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Security.AccessControl;
using System.Security.Cryptography;
using System.Security.Principal;
namespace NetLinkServiceCheck {
 public sealed partial class WindowsHost {
  static string ToolsRoot {get{return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),"NetLink Service Tools");}}
  static string HelperRoot {get{return Path.Combine(ToolsRoot,StartupPayload.Sha256);}}
  static string Helper {get{return Path.Combine(HelperRoot,"netlink-startup-check.exe");}}
  static byte[] Payload(){using(var input=Assembly.GetExecutingAssembly().GetManifestResourceStream("NetLink.StartupCheck")){if(input==null)throw new InvalidDataException("Controllo nativo non incluso in questa build.");using(var output=new MemoryStream()){input.CopyTo(output);return output.ToArray();}}}
  static string Hash(byte[] value){using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(value)).Replace("-","");}
  static void Protected(string path,bool directory) {
   NoLinks(path);
   var security=directory?(FileSystemSecurity)Directory.GetAccessControl(path):File.GetAccessControl(path);
   var admin=new SecurityIdentifier(WellKnownSidType.BuiltinAdministratorsSid,null);var system=new SecurityIdentifier(WellKnownSidType.LocalSystemSid,null);
   var installer=new SecurityIdentifier("S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464");
   const FileSystemRights write=FileSystemRights.Write|FileSystemRights.Delete|FileSystemRights.DeleteSubdirectoriesAndFiles|FileSystemRights.ChangePermissions|FileSystemRights.TakeOwnership;
   var owner=security.GetOwner(typeof(SecurityIdentifier));if(!owner.Equals(admin)&&!owner.Equals(system)&&!owner.Equals(installer))throw new InvalidDataException("Proprietario del percorso del controllo non protetto.");
   foreach(FileSystemAccessRule rule in security.GetAccessRules(true,true,typeof(SecurityIdentifier))) {
    if(rule.PropagationFlags==PropagationFlags.InheritOnly||rule.AccessControlType!=AccessControlType.Allow||(rule.FileSystemRights&write)==0)continue;
    if(!rule.IdentityReference.Equals(admin)&&!rule.IdentityReference.Equals(system)&&!rule.IdentityReference.Equals(installer))throw new InvalidDataException("Il controllo non può essere eseguito da un percorso modificabile da utenti non amministratori.");
   }
  }
  static void CheckHelper(string path) {
   Protected(path,false);for(var parent=Path.GetDirectoryName(path);parent!=null;parent=Path.GetDirectoryName(parent)) {
    Protected(parent,true);if(parent.Equals(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),StringComparison.OrdinalIgnoreCase))break;
   }
   if(Hash(File.ReadAllBytes(path))!=StartupPayload.Sha256)throw new InvalidDataException("Il controllo nativo presente non corrisponde a questa utilità. Premi Imposta controllo per installare la copia inclusa.");
  }
  static string ExecuteHelper(string path,string arguments) {
   CheckHelper(path);var start=new ProcessStartInfo(path,arguments){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,WorkingDirectory=Path.GetDirectoryName(path)};
   using(var process=Process.Start(start)) {
    // Output is bounded by the native helper (configuration/status only).
    if(!process.WaitForExit(40000))throw new TimeoutException("Il controllo nativo è ancora occupato. Non viene terminato forzatamente; riprova dopo un minuto.");
    var output=process.StandardOutput.ReadToEnd();var error=process.StandardError.ReadToEnd();
    if(process.ExitCode!=0)throw new InvalidOperationException(error+" (codice "+process.ExitCode+")");return output.Trim();
   }
  }
  static string ExistingHelper() {
   if(File.Exists(Helper))return Helper;
   var suite=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),"Veyon School","netlink-startup-check.exe");return File.Exists(suite)?suite:null;
  }
  static void ReadStartupCheck(Snapshot snapshot) {
   try {
    var path=ExistingHelper();if(path==null){snapshot.StartupInfo="Controllo nativo non installato. Puoi configurarlo con questa utilità anche su NetLink precedente ad alpha.38.";return;}
    var text=ExecuteHelper(path,"--status");int delay;bool task=text.EndsWith("task=present",StringComparison.Ordinal);
    var parts=text.Split(';');if(parts.Length!=2||!parts[0].StartsWith("delay=",StringComparison.Ordinal)||!Int32.TryParse(parts[0].Substring(6),out delay)||(delay!=0&&delay!=3&&delay!=5))throw new InvalidDataException("Risposta diagnostica inattesa.");
    snapshot.StartupDelay=delay;snapshot.StartupTask=task;snapshot.StartupInfo="Attività Windows: NetLink Service Startup Check. "+(task?"Eseguita come SYSTEM, senza una finestra o un utente connesso.":"Attività assente; il servizio continua a usare il normale avvio Windows.");
   }catch(Exception error){snapshot.StartupInfo=Engine.Explain(error);}
  }
  static void EnsureProtectedDirectory(string path) {
   NoLinks(path,true);Protected(Path.GetDirectoryName(path),true);
   if(Directory.Exists(path))Protected(path,true);else Directory.CreateDirectory(path);
   NoLinks(path);
   var acl=new DirectorySecurity();acl.SetAccessRuleProtection(true,false);
   acl.SetOwner(new SecurityIdentifier(WellKnownSidType.BuiltinAdministratorsSid,null));
   foreach(var type in new[]{WellKnownSidType.BuiltinAdministratorsSid,WellKnownSidType.LocalSystemSid})acl.AddAccessRule(new FileSystemAccessRule(new SecurityIdentifier(type,null),FileSystemRights.FullControl,InheritanceFlags.ContainerInherit|InheritanceFlags.ObjectInherit,PropagationFlags.None,AccessControlType.Allow));
   acl.AddAccessRule(new FileSystemAccessRule(new SecurityIdentifier(WellKnownSidType.BuiltinUsersSid,null),FileSystemRights.ReadAndExecute,InheritanceFlags.ContainerInherit|InheritanceFlags.ObjectInherit,PropagationFlags.None,AccessControlType.Allow));
   Directory.SetAccessControl(path,acl);NoLinks(path);
  }
  public void ConfigureStartupCheck(int minutes) {
   if(minutes!=0&&minutes!=3&&minutes!=5)throw new ArgumentOutOfRangeException("minutes");
   bool created;using(var maintenance=new System.Threading.Mutex(false,"Global\\NetLinkInstallerMaintenance",out created)) {
   if(!created)throw new InvalidOperationException("Installazione, disinstallazione o configurazione del controllo già in corso. Riprova al termine.");
   var current=Read();if(!current.Exists||!current.Trusted||!current.ConfigKnown)throw new InvalidOperationException("Servizio NetLink non riconosciuto.");
   // A portable EXE in Downloads must never become a SYSTEM task action.
   // Immutable hash directories also avoid replacing a running older helper.
   EnsureProtectedDirectory(ToolsRoot);EnsureProtectedDirectory(HelperRoot);
   var payload=Payload();if(Hash(payload)!=StartupPayload.Sha256)throw new InvalidDataException("Controllo incluso danneggiato.");
   if(File.Exists(Helper)){Protected(Helper,false);if(Hash(File.ReadAllBytes(Helper))!=StartupPayload.Sha256)throw new InvalidOperationException("È già presente un'altra versione del controllo nativo in NetLink Service Tools. Non viene sovrascritta: usa l'installer NetLink aggiornato o rimuovi il vecchio controllo da amministratore.");}
   else {using(var file=new FileStream(Helper,FileMode.CreateNew,FileAccess.Write,FileShare.None))file.Write(payload,0,payload.Length);var security=File.GetAccessControl(Helper);security.SetOwner(new SecurityIdentifier(WellKnownSidType.BuiltinAdministratorsSid,null));File.SetAccessControl(Helper,security);}
   ExecuteHelper(Helper,"--configure "+(minutes==0?"off":minutes.ToString()));
   }
  }
 }
}
