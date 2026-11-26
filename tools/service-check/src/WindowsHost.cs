// SPDX-License-Identifier: GPL-2.0-or-later
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Diagnostics.Eventing.Reader;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;
using Microsoft.Win32.SafeHandles;

namespace NetLinkServiceCheck {
 public sealed partial class WindowsHost:IHost,IStartupCheck {
  const string ServiceName="VeyonSchoolService";
  const uint QueryConfig=1,ChangeConfig=2,QueryStatus=4,StartAccess=16;
  sealed class Handle:SafeHandleZeroOrMinusOneIsInvalid {public Handle():base(true){} protected override bool ReleaseHandle(){return CloseServiceHandle(handle);} }
  [StructLayout(LayoutKind.Sequential)] struct Config {public uint Type,Start,Error;public IntPtr Image,Group;public uint Tag;public IntPtr Dependencies,Account,Display;}
  [StructLayout(LayoutKind.Sequential)] struct Status {public uint Type,State,Controls,Exit,Specific,CheckPoint,WaitHint,Pid,Flags;}
  [StructLayout(LayoutKind.Sequential)] struct Action {public uint Type,Delay;}
  [StructLayout(LayoutKind.Sequential)] struct Failures {public uint Reset;public IntPtr Reboot,Command;public uint Count;public IntPtr Actions;}
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern Handle OpenSCManager(string machine,string database,uint access);
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern Handle OpenService(Handle manager,string name,uint access);
  [DllImport("advapi32.dll",SetLastError=true)] static extern bool CloseServiceHandle(IntPtr handle);
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool QueryServiceConfig(Handle service,IntPtr buffer,uint size,out uint needed);
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool QueryServiceConfig2(Handle service,uint level,IntPtr buffer,uint size,out uint needed);
  [DllImport("advapi32.dll",SetLastError=true)] static extern bool QueryServiceStatusEx(Handle service,int level,out Status status,uint size,out uint needed);
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool ChangeServiceConfig(Handle service,uint type,uint start,uint error,string image,string group,IntPtr tag,string dependencies,string account,string password,string display);
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool ChangeServiceConfig2(Handle service,uint level,IntPtr info);
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool StartService(Handle service,uint count,IntPtr arguments);
  [DllImport("kernel32.dll")] static extern ulong GetTickCount64();
  static void Require(bool success){if(!success)throw new Win32Exception(Marshal.GetLastWin32Error());}
  static Handle Manager(){var manager=OpenSCManager(null,null,1);if(manager.IsInvalid){var e=Marshal.GetLastWin32Error();manager.Dispose();throw new Win32Exception(e);}return manager;}
  static Handle Service(Handle manager,uint access){var service=OpenService(manager,ServiceName,access);if(service.IsInvalid){var e=Marshal.GetLastWin32Error();service.Dispose();throw new Win32Exception(e);}return service;}
  static IntPtr Buffer(Handle service,uint level,bool main,out uint size) {
   uint needed;bool ok=main?QueryServiceConfig(service,IntPtr.Zero,0,out needed):QueryServiceConfig2(service,level,IntPtr.Zero,0,out needed);
   if(!ok && Marshal.GetLastWin32Error()!=122)throw new Win32Exception(Marshal.GetLastWin32Error());
   if(needed<4 || needed>65536)throw new InvalidDataException("Dimensione inattesa dei parametri del servizio.");
   var buffer=Marshal.AllocHGlobal((int)needed);size=needed;
   try{Require(main?QueryServiceConfig(service,buffer,size,out needed):QueryServiceConfig2(service,level,buffer,size,out needed));return buffer;}catch{Marshal.FreeHGlobal(buffer);throw;}
  }
  static string StringAt(IntPtr pointer,IntPtr buffer,uint bytes) {
   if(pointer==IntPtr.Zero)return "";long offset=pointer.ToInt64()-buffer.ToInt64();
   if(offset<0 || offset>=bytes || offset%2!=0)throw new InvalidDataException("Puntatore SCM non valido.");
   var text=new StringBuilder();for(long i=offset;i+1<bytes;i+=2){char c=(char)Marshal.ReadInt16(buffer,(int)i);if(c==0)return text.ToString();text.Append(c);}
   throw new InvalidDataException("Stringa SCM non terminata.");
  }
  static Status ReadStatus(Handle service){Status status;uint needed;Require(QueryServiceStatusEx(service,0,out status,(uint)Marshal.SizeOf(typeof(Status)),out needed));return status;}
  static bool RecoveryMatches(Handle service) {
   uint size;var b=Buffer(service,2,false,out size);
   try {
    var f=(Failures)Marshal.PtrToStructure(b,typeof(Failures));
    if(f.Reset!=86400 || f.Count!=4)return false;
    long offset=f.Actions.ToInt64()-b.ToInt64();if(offset<0 || offset+32>size)return false;
    uint[] delays={10000,30000,60000,0};
    for(int i=0;i<4;i++){var a=(Action)Marshal.PtrToStructure(IntPtr.Add(f.Actions,8*i),typeof(Action));if(a.Type!=(i==3?0u:1u)||a.Delay!=delays[i])return false;}
    // A restart-only profile must not retain an external command/reboot text.
    if(StringAt(f.Command,b,size).Length>0 || StringAt(f.Reboot,b,size).Length>0)return false;
   } finally {Marshal.FreeHGlobal(b);}
   b=Buffer(service,4,false,out size);try{return Marshal.ReadInt32(b)!=0;}finally{Marshal.FreeHGlobal(b);}
  }
  static Snapshot Core(Handle service) {
   var s=new Snapshot{Exists=true,Boot=DateTime.Now-TimeSpan.FromMilliseconds(GetTickCount64())};
   uint size;var buffer=Buffer(service,0,true,out size);
   try {
    var c=(Config)Marshal.PtrToStructure(buffer,typeof(Config));s.StartType=c.Start;
    s.Image=StringAt(c.Image,buffer,size);s.Account=StringAt(c.Account,buffer,size);s.Display=StringAt(c.Display,buffer,size);s.Group=StringAt(c.Group,buffer,size);
    var dependencies=new List<string>();var p=c.Dependencies;
    while(p!=IntPtr.Zero){var name=StringAt(p,buffer,size);if(name.Length==0)break;dependencies.Add(name);if(dependencies.Count>128)throw new InvalidDataException("Troppe dipendenze SCM.");p=IntPtr.Add(p,(name.Length+1)*2);}
    s.Dependencies=dependencies.ToArray();
    if((c.Type&0x30)!=0x10)s.IdentityReason="Il servizio non è un processo Win32 autonomo previsto per NetLink.";
   }finally{Marshal.FreeHGlobal(buffer);}
   var state=ReadStatus(service);s.State=state.State;s.Pid=state.Pid;s.ExitCode=state.Exit;s.SpecificExitCode=state.Specific;
   var warnings=new List<string>();
   try{buffer=Buffer(service,3,false,out size);try{s.Delayed=Marshal.ReadInt32(buffer)!=0;}finally{Marshal.FreeHGlobal(buffer);}}catch(Exception e){s.ConfigKnown=false;warnings.Add("Avvio ritardato non leggibile: "+Engine.Explain(e));}
   try{s.Recovery=RecoveryMatches(service);}catch(Exception e){warnings.Add("Recupero non leggibile: "+Engine.Explain(e));}
   try{buffer=Buffer(service,8,false,out size);try{s.TriggerCount=(uint)Marshal.ReadInt32(buffer);}finally{Marshal.FreeHGlobal(buffer);}}catch(Exception e){warnings.Add("Trigger non leggibili: "+Engine.Explain(e));}
   if(s.IdentityReason.Length==0) {
    try {
     s.Executable=ParseImage(s.Image);ValidatePath(s.Executable);
     var version=FileVersionInfo.GetVersionInfo(s.Executable);s.Version=version.FileVersion??"";
     if(!s.Version.ToLowerInvariant().Contains("netlink") && !(version.ProductName??"").ToLowerInvariant().Contains("netlink"))throw new InvalidDataException("Metadati NetLink non riconosciuti: potrebbe essere Veyon originale o un file estraneo.");
     s.Trusted=true;s.IdentityReason="Percorso protetto, file presente e metadati NetLink riconosciuti. Non è una verifica della firma digitale.";
    }catch(Exception e){s.IdentityReason=e.Message;}
   }
   s.Warnings=warnings.ToArray();return s;
  }
  public static string ParseImage(string image) {
   var input=Environment.ExpandEnvironmentVariables(image??"").Trim();
   if(input.StartsWith("\"",StringComparison.Ordinal)){int end=input.IndexOf('"',1);if(end<=1 || input.Substring(end+1).Trim().Length!=0)throw new InvalidDataException("Comando del servizio inatteso: non vengono eseguiti argomenti aggiuntivi.");return input.Substring(1,end-1);}
   if(input.Any(Char.IsWhiteSpace))throw new InvalidDataException("Percorso del servizio senza virgolette o con argomenti: correggere con l'installer NetLink, non alla cieca.");
   if(input.Length==0)throw new InvalidDataException("Percorso del servizio vuoto.");return input;
  }
  public static void NoLinks(string path,bool missingLeaf=false) {
   bool leaf=true;
   for(var p=Path.GetFullPath(path);!String.IsNullOrEmpty(p);p=Path.GetDirectoryName(p)) {
    try {if((File.GetAttributes(p)&FileAttributes.ReparsePoint)!=0)throw new InvalidDataException("Percorso con collegamenti o reindirizzamenti: operazione prudenzialmente bloccata.");}
    catch(FileNotFoundException){if(!leaf||!missingLeaf)throw;}
    catch(DirectoryNotFoundException){if(!leaf||!missingLeaf)throw;}
    leaf=false;
   }
  }
  static void ValidatePath(string path) {
   var full=Path.GetFullPath(path);var name=Path.GetFileName(full);
   if(!Path.IsPathRooted(path) || (!name.Equals("netlink-service.exe",StringComparison.OrdinalIgnoreCase) && !name.Equals("veyon-service.exe",StringComparison.OrdinalIgnoreCase)))throw new InvalidDataException("Eseguibile del servizio estraneo a NetLink.");
   var pf=Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
   bool allowed=new[]{"Veyon School","NetLink School"}.Any(folder=>Path.GetDirectoryName(full).Equals(Path.Combine(pf,folder),StringComparison.OrdinalIgnoreCase));
   if(!allowed)throw new InvalidDataException("Installazione in un percorso non standard: sola diagnosi. Usare l'installer NetLink per la riparazione.");
   if(!File.Exists(full))throw new FileNotFoundException("Eseguibile del servizio mancante.",full);NoLinks(full);
  }
  public Snapshot Read() {
   Snapshot s;
   try{using(var manager=Manager())using(var service=Service(manager,QueryConfig|QueryStatus))s=Core(service);}
   catch(Win32Exception e){return new Snapshot{Exists=false,ConfigKnown=e.NativeErrorCode==1060,ReadError=Engine.Explain(e)};}
   catch(Exception e){return new Snapshot{ConfigKnown=false,ReadError=e.Message};}
   var warnings=s.Warnings.ToList();var dependencyProblems=new List<string>();
   // Read only this service preference, never authentication material. A later
   // Configurator Apply otherwise legitimately restores the stored Manual choice.
   try {using(var key=Microsoft.Win32.Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Veyon Solutions\Veyon\Service")) {
    var value=key==null?null:key.GetValue("Autostart");
    if(value!=null && (value.ToString()=="0"||value.ToString().Equals("false",StringComparison.OrdinalIgnoreCase)))warnings.Add("Nel Configurator l'avvio automatico del servizio è disattivato. Una futura applicazione della configurazione può riportare il servizio a Manuale. L'utilità non cambia questa scelta salvata: abilitala nel Configurator se desideri mantenerlo Automatico.");
   }}catch(Exception e){warnings.Add("Preferenza di avvio del Configurator non leggibile: "+e.Message);}
   using(var manager=Manager())foreach(var dependency in s.Dependencies) {
    if(dependency.StartsWith("+",StringComparison.Ordinal)){dependencyProblems.Add(dependency+": gruppo di caricamento; verifica manuale richiesta.");continue;}
    using(var service=OpenService(manager,dependency,QueryConfig|QueryStatus)) {
     if(service.IsInvalid){dependencyProblems.Add(dependency+": non leggibile (errore "+Marshal.GetLastWin32Error()+").");continue;}
     try {uint size;var b=Buffer(service,0,true,out size);try{var c=(Config)Marshal.PtrToStructure(b,typeof(Config));if(c.Start==4)dependencyProblems.Add(dependency+": disabilitata.");}finally{Marshal.FreeHGlobal(b);}
      var st=ReadStatus(service);if(st.State!=4)dependencyProblems.Add(dependency+": non in esecuzione (stato "+st.State+").");
     }catch(Exception e){dependencyProblems.Add(dependency+": "+Engine.Explain(e));}
    }
   }
   s.DependencyProblems=dependencyProblems.ToArray();
   ReadStartupCheck(s);
   try {
    const string query="*[System[Provider[@Name='Service Control Manager'] and TimeCreated[timediff(@SystemTime) <= 604800000] and (EventID=7000 or EventID=7001 or EventID=7003 or EventID=7009 or EventID=7011 or EventID=7022 or EventID=7023 or EventID=7024 or EventID=7031 or EventID=7034 or EventID=7036 or EventID=7038 or EventID=7040 or EventID=7041)]]";
    var q=new EventLogQuery("System",PathType.LogName,query){ReverseDirection=true};var events=new List<Evidence>();
    using(var reader=new EventLogReader(q))for(int i=0;i<1500 && events.Count<30;i++)using(var entry=reader.ReadEvent()) {
     if(entry==null)break;
     if(!entry.Properties.Any(p=>String.Equals(p.Value as string,s.Name,StringComparison.OrdinalIgnoreCase)||String.Equals(p.Value as string,s.Display,StringComparison.OrdinalIgnoreCase)))continue;
     string message;try{message=entry.FormatDescription();}catch{message="Descrizione non disponibile. Consultare il registro Sistema, evento "+entry.Id+".";}
     events.Add(new Evidence{Id=entry.Id,Time=entry.TimeCreated??DateTime.MinValue,Message=(message??"").Substring(0,Math.Min((message??"").Length,5000))});
    }
    s.Events=events.ToArray();
   }catch(Exception e){warnings.Add("Registro Sistema non leggibile: "+Engine.Explain(e));}
   s.Warnings=warnings.ToArray();return s;
  }
  public string Backup(Snapshot s) {
   var root=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"NetLink","ServiceCheck","Backups");
   for(var p=Path.GetFullPath(root);!String.IsNullOrEmpty(p);p=Path.GetDirectoryName(p)) {
    try{if((File.GetAttributes(p)&FileAttributes.ReparsePoint)!=0)throw new InvalidDataException("Percorso di backup reindirizzato: operazione bloccata.");}
    catch(FileNotFoundException){}catch(DirectoryNotFoundException){}
   }
   Directory.CreateDirectory(root);NoLinks(root);
   var security=new DirectorySecurity();security.SetAccessRuleProtection(true,false);
   foreach(var sid in new[]{WindowsIdentity.GetCurrent().User,new SecurityIdentifier(WellKnownSidType.LocalSystemSid,null),new SecurityIdentifier(WellKnownSidType.BuiltinAdministratorsSid,null)})
    security.AddAccessRule(new FileSystemAccessRule(sid,FileSystemRights.FullControl,InheritanceFlags.ContainerInherit|InheritanceFlags.ObjectInherit,PropagationFlags.None,AccessControlType.Allow));
   Directory.SetAccessControl(root,security);
   string path=Path.Combine(root,DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N")+".json");
   using(var stream=new FileStream(path,FileMode.CreateNew,FileAccess.Write,FileShare.None))using(var writer=new StreamWriter(stream,new UTF8Encoding(false)))writer.Write(new JavaScriptSerializer().Serialize(s));
   return path;
  }
  void Write(System.Action<Handle> action) {
   using(var manager=Manager())using(var service=Service(manager,QueryConfig|QueryStatus|ChangeConfig|StartAccess)) {
    var s=Core(service);if(!s.Trusted || !s.ConfigKnown)throw new InvalidOperationException("Identità o parametri non verificabili: nessuna modifica eseguita. "+s.IdentityReason);
    action(service);
   }
  }
  public void SetAutomatic(){Write(s=>Require(ChangeServiceConfig(s,UInt32.MaxValue,2,UInt32.MaxValue,null,null,IntPtr.Zero,null,null,null,null)));}
  public void SetDelay(bool delayed){Write(s=>{var p=Marshal.AllocHGlobal(4);try{Marshal.WriteInt32(p,delayed?1:0);Require(ChangeServiceConfig2(s,3,p));}finally{Marshal.FreeHGlobal(p);}});}
  public void SetRecovery(){Write(s=>{
   var actions=Marshal.AllocHGlobal(32);var empty=Marshal.StringToHGlobalUni("");var ptr=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(Failures)));
   try {
    uint[] delays={10000,30000,60000,0};for(int i=0;i<4;i++)Marshal.StructureToPtr(new Action{Type=i==3?0u:1u,Delay=delays[i]},IntPtr.Add(actions,i*8),false);
    var f=new Failures{Reset=86400,Count=4,Actions=actions,Command=empty,Reboot=empty};Marshal.StructureToPtr(f,ptr,false);Require(ChangeServiceConfig2(s,2,ptr));
    Marshal.WriteInt32(ptr,1);Require(ChangeServiceConfig2(s,4,ptr));
   }finally{Marshal.FreeHGlobal(ptr);Marshal.FreeHGlobal(empty);Marshal.FreeHGlobal(actions);}
  });}
  public void Start(){bool fresh;using(var lease=new System.Threading.Mutex(false,@"Global\NetLinkInstallerMaintenance",out fresh)){if(!fresh)throw new InvalidOperationException("Manutenzione già in corso.");System.Threading.Mutex prep;if(System.Threading.Mutex.TryOpenExisting(@"Global\NetLinkUpgradePreparation",out prep)){prep.Dispose();throw new InvalidOperationException("Preparazione aggiornamento in corso.");}Write(s=>{
   if(!StartService(s,0,IntPtr.Zero)){var error=Marshal.GetLastWin32Error();if(error!=1056)throw new Win32Exception(error);}
   var clock=Stopwatch.StartNew();while(clock.ElapsedMilliseconds<120000){var status=ReadStatus(s);if(status.State==4)return;if(status.State==1)throw new Win32Exception((int)(status.Exit==0?1067:status.Exit));Thread.Sleep(200);}
   throw new Win32Exception(1460);
  });}}
 }
}
