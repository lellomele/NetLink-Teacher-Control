// SPDX-License-Identifier: GPL-2.0-or-later
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using NetLinkServiceCheck;
static class Tests {
 static int count;
 static void Assert(bool value,string message){if(!value)throw new Exception(message);++count;Console.WriteLine("PASS "+message);}
 sealed class Fake:IHost,IStartupCheck {
  public Snapshot Data=new Snapshot{Exists=true,Trusted=true,StartType=3,State=1,Image="\"C:\\Program Files\\Veyon School\\netlink-service.exe\"",Account="LocalSystem",Version="1.0.0-alpha.37",IdentityReason="Fixture",Group="unchanged",Dependencies=new[]{"dependency"}};
  public string Fail="";public bool Ignore;public List<string> Calls=new List<string>();
  public Snapshot Read(){return new JavaScriptSerializer().Deserialize<Snapshot>(new JavaScriptSerializer().Serialize(Data));}
  void Call(string name){Calls.Add(name);if(Fail==name)throw new Win32Exception(5);}
  public string Backup(Snapshot s){Call("backup");return "fixture-only.json";}
  public void SetAutomatic(){Call("automatic");if(!Ignore)Data.StartType=2;}
  public void SetDelay(bool delayed){Call("delay");if(!Ignore)Data.Delayed=delayed;}
  public void SetRecovery(){Call("recovery");if(!Ignore)Data.Recovery=true;}
  public void Start(){Call("start");if(!Ignore)Data.State=4;}
  public void ConfigureStartupCheck(int minutes){Call("startup");if(!Ignore){Data.StartupDelay=minutes;Data.StartupTask=minutes!=0;}}
 }
 [STAThread] static int Main(string[] args){try {
  if(args.Length==2&&args[0]=="--diagnose"){File.WriteAllText(args[1],MainForm.Report(new WindowsHost().Read()));return 0;}
  // Explicit VM-only integration entry, not part of the distributed app CLI.
  if(args.Length==2&&args[0]=="--configure-startup") {
   if(Environment.MachineName!="NETLINK-RMTEST")throw new InvalidOperationException("Integration writes are restricted to the dedicated VM.");
   int minutes;if(!Int32.TryParse(args[1],out minutes))return 2;var host=new WindowsHost();var outcome=new Engine(host).ApplyStartupCheck(host.Read(),minutes);Console.WriteLine(outcome.Message);return outcome.Success?0:1;
  }
  string root=Path.GetFullPath(args[0]);Directory.CreateDirectory(root);
  var missing=Path.Combine(root,"missing-"+Guid.NewGuid().ToString("N"));WindowsHost.NoLinks(missing,true);Assert(!Directory.Exists(missing),"Missing helper leaf is validated without creating it");
  bool missingRejected=false;try{WindowsHost.NoLinks(missing);}catch(FileNotFoundException){missingRejected=true;}catch(DirectoryNotFoundException){missingRejected=true;}Assert(missingRejected,"Existing-path validation still refuses a missing path");
  missingRejected=false;try{WindowsHost.NoLinks(Path.Combine(missing,"child"),true);}catch(FileNotFoundException){missingRejected=true;}catch(DirectoryNotFoundException){missingRejected=true;}Assert(missingRejected,"Missing ancestor is never accepted for deployment");
  var f=new Fake();var engine=new Engine(f);var before=engine.Analyze().Snapshot;Assert(f.Calls.Count==0,"Analysis never writes");
  var result=engine.Apply(before,true,false);Assert(result.Success,"Automatic non-delayed profile verified");Assert(String.Join(",",f.Calls)=="backup,automatic,delay,recovery","Backup before every setting change");Assert(f.Data.Account=="LocalSystem"&&f.Data.Group=="unchanged"&&f.Data.Dependencies.SequenceEqual(new[]{"dependency"}),"Account, group and dependencies preserved");
  foreach(int minutes in new[]{3,5,0}){f=new Fake();engine=new Engine(f);result=engine.ApplyStartupCheck(f.Read(),minutes);Assert(result.Success&&f.Data.StartupDelay==minutes,"Boot-check delay verified: "+minutes);Assert(String.Join(",",f.Calls)=="backup,startup","Backup before task configuration; no service mutation");Assert(f.Data.StartType==3&&f.Data.State==1,"Manual service is not started by task configuration");}
  f=new Fake{Fail="backup"};engine=new Engine(f);result=engine.ApplyStartupCheck(f.Read(),3);Assert(!result.Success&&!f.Calls.Contains("startup"),"Backup failure prevents task mutation");
  f=new Fake{Ignore=true};engine=new Engine(f);result=engine.ApplyStartupCheck(f.Read(),5);Assert(!result.Success,"Ignored task configuration never reports success");
  f=new Fake();engine=new Engine(f);result=engine.ApplyStartupCheck(f.Read(),4);Assert(!result.Success&&f.Calls.Count==0,"Invalid boot delay rejected without writes");
  f=new Fake();engine=new Engine(f);result=engine.Apply(f.Read(),false,false,true);Assert(result.Success&&f.Data.Delayed,"Delayed alternative verified");Assert(!f.Calls.Contains("recovery"),"Existing recovery retained when unchecked");Assert(!f.Calls.Contains("start"),"Changing startup does not start the service");
  f=new Fake();engine=new Engine(f);result=engine.Apply(f.Read(),false,true);Assert(result.Success&&f.Data.State==4,"Explicit start reaches running");Assert(String.Join(",",f.Calls)=="backup,start","Starting does not change startup settings");
  foreach(string fault in new[]{"missing","foreign","unknown","changed","disabled-start","transition","backup"}) {
   f=new Fake();engine=new Engine(f);var approved=f.Read();bool start=false;
   if(fault=="missing")f.Data.Exists=false;if(fault=="foreign")f.Data.Trusted=false;if(fault=="unknown")f.Data.ConfigKnown=false;if(fault=="changed")f.Data.Image="changed";
   if(fault=="disabled-start"){f.Data.StartType=4;approved=f.Read();start=true;}if(fault=="transition"){f.Data.State=2;approved=f.Read();start=true;}if(fault=="backup")f.Fail="backup";
   result=engine.Apply(approved,true,start);Assert(!result.Success,"Refuses "+fault);Assert(f.Calls.All(c=>c=="backup"),"No mutations after "+fault);
  }
  foreach(string fault in new[]{"automatic","delay","recovery","start"}) {
   f=new Fake{Fail=fault};engine=new Engine(f);result=engine.Apply(f.Read(),true,fault=="start");Assert(!result.Success,"Propagates "+fault+" failure");Assert(f.Calls[0]=="backup"&&f.Calls.Last()==fault,"Stops at failed "+fault+" step");Assert(result.Message.Contains("Errore Windows 5"),"Reports exact native error");
  }
  f=new Fake{Ignore=true};engine=new Engine(f);result=engine.Apply(f.Read(),true,false);Assert(!result.Success,"Does not report success for ignored settings");
  f=new Fake();f.Data.State=4;engine=new Engine(f);result=engine.Apply(f.Read(),false,true);Assert(result.Success&&f.Calls.Count==0,"Already running is a non-mutating no-op");
  var s=new Snapshot{Exists=true,Trusted=true,StartType=2,State=1,Boot=DateTime.Now.AddHours(-1),Events=new[]{new Evidence{Id=7009,Time=DateTime.Now,Message="Timeout fixture"},new Evidence{Id=7040,Time=DateTime.Now,Message="Changed fixture"}}};
  var checks=Engine.Checks(s);Assert(checks.Any(c=>c.Title=="Automatico ma fermo"),"Distinguishes automatic from running");Assert(checks.Any(c=>c.Title=="Timeout nell'avvio attuale"),"Reports current-boot timeout evidence");Assert(checks.Any(c=>c.Title=="Tipo di avvio modificato"),"Reports configuration changes without blaming another product");
  s.Events[0].Time=DateTime.Now.AddDays(-1);Assert(!Engine.Checks(s).Any(c=>c.Title=="Timeout nell'avvio attuale"),"Old timeout not attributed to this boot");
  Assert(WindowsHost.ParseImage("\"C:\\Program Files\\Veyon School\\netlink-service.exe\"")=="C:\\Program Files\\Veyon School\\netlink-service.exe","Quoted image parsed");
  foreach(string bad in new[]{"", "C:\\Program Files\\Veyon School\\netlink-service.exe","\"C:\\a.exe\" -foreign", "\"unfinished"}){bool rejected=false;try{WindowsHost.ParseImage(bad);}catch{rejected=true;}Assert(rejected,"Rejects ambiguous image: "+bad);}
  foreach(int error in new[]{5,1060,1072,1077,1058,1053,1068,1069,2,193})Assert(Engine.Explain(new Win32Exception(error)).Contains(error.ToString()),"Native error "+error+" kept in diagnosis");
  Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
  using(var form=new MainForm(new Fake(),false)) {s.Version="1.0.0-alpha.37";form.ShowSnapshot(s);form.ShowInTaskbar=false;form.StartPosition=FormStartPosition.Manual;form.Location=new Point(-30000,-30000);form.Show();Application.DoEvents();using(var bitmap=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(bitmap,new Rectangle(Point.Empty,bitmap.Size));bitmap.Save(Path.Combine(root,"preview.png"));}Assert(form.WindowState==FormWindowState.Normal,"Does not start maximized");Assert(form.Icon!=null,"Colored application icon present");form.Close();}
  foreach(float scale in new[]{1f,1.5f,2f})using(var form=new MainForm(new Fake(),false)) {
   form.ShowSnapshot(s);form.ShowInTaskbar=false;form.StartPosition=FormStartPosition.Manual;form.Location=new Point(-30000,-30000);form.Scale(new SizeF(scale,scale));form.Size=new Size((int)(760*scale),(int)(620*scale));form.Show();Application.DoEvents();
   foreach(var button in Descendants(form).OfType<Button>()){var bounds=form.RectangleToClient(button.Parent.RectangleToScreen(button.Bounds));Assert(form.ClientRectangle.Contains(bounds),"Action visible at scale "+scale+": "+button.Text);}
   form.Close();
  }
  Console.WriteLine("TOTAL "+count+" checks passed; no installed service was changed.");return 0;
 }catch(Exception e){Console.Error.WriteLine(e);return 1;}}
 static IEnumerable<Control> Descendants(Control parent){foreach(Control child in parent.Controls){yield return child;foreach(var nested in Descendants(child))yield return nested;}}
}
