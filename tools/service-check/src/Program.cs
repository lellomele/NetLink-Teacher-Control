// SPDX-License-Identifier: GPL-2.0-or-later
using System;
using System.ComponentModel;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Text;
using System.Web.Script.Serialization;
using System.Windows.Forms;

[assembly:AssemblyTitle("NetLink — Controllo servizio")]
[assembly:AssemblyProduct("NetLink Service Check")]
[assembly:AssemblyCompany("prof. Ing. Raffaele Mele")]
[assembly:AssemblyCopyright("© 2026 prof. Ing. Raffaele Mele")]
[assembly:AssemblyVersion("1.2.0.0")]
[assembly:AssemblyFileVersion("1.2.0.0")]
namespace NetLinkServiceCheck {
 public sealed class MainForm:Form {
  readonly Engine engine;Snapshot current;
  readonly BackgroundWorker worker=new BackgroundWorker();
  readonly ListView checks=new ListView{View=View.Details,FullRowSelect=true,GridLines=false,HideSelection=false,ShowItemToolTips=true,Dock=DockStyle.Fill,BorderStyle=BorderStyle.None};
  readonly Label headline=new Label{AutoSize=true,Font=new Font("Segoe UI",16,FontStyle.Bold),ForeColor=Color.FromArgb(27,61,105)};
  readonly Label status=new Label{AutoSize=true,Text="Nessuna modifica eseguita.",ForeColor=Color.FromArgb(52,75,100),MaximumSize=new Size(900,0)};
  readonly Label summary=new Label{AutoSize=true,ForeColor=Color.FromArgb(44,65,90)};
  readonly Button analyze=Button("Analizza",true),configure=Button("Imposta avvio…",false),start=Button("Avvia servizio…",false),export=Button("Salva rapporto…",false);
  readonly ComboBox mode=new ComboBox{DropDownStyle=ComboBoxStyle.DropDownList,Width=340};
  readonly ComboBox startupDelay=new ComboBox{DropDownStyle=ComboBoxStyle.DropDownList,Width=155};
  readonly Button startup=Button("Imposta controllo…",false);
  readonly CheckBox recovery=new CheckBox{Text="Imposta anche il recupero dopo errore (3 tentativi, poi stop)",AutoSize=true,Checked=true};
  readonly CheckBox refresh=new CheckBox{Text="Aggiorna ogni 15 secondi (sola lettura)",AutoSize=true};
  readonly Timer timer=new Timer{Interval=15000};readonly ToolTip tips=new ToolTip{AutoPopDelay=9000,InitialDelay=500,ReshowDelay=150};
  public MainForm():this(new WindowsHost(),true){}
  public MainForm(IHost host,bool automatic) {
   engine=new Engine(host);Text="NetLink · Controllo servizio 1.2.0";Font=new Font("Segoe UI",10);BackColor=Color.FromArgb(241,245,250);
   AutoScaleMode=AutoScaleMode.Dpi;AutoScaleDimensions=new SizeF(96,96);StartPosition=FormStartPosition.CenterScreen;ClientSize=new Size(1040,720);MinimumSize=new Size(700,470);
   Icon=Icon.ExtractAssociatedIcon(Application.ExecutablePath);
   var menu=new MenuStrip{BackColor=BackColor};var info=new ToolStripMenuItem("Informazioni");menu.Items.Add(info);info.Click+=(s,e)=>MessageBox.Show(this,"NetLink Service Check 1.2.0\r\n© 2026 prof. Ing. Raffaele Mele\r\nGPL-2.0-or-later\r\n\r\nControlla esclusivamente il servizio locale NetLink.\r\nNon modifica altri servizi, configurazione o chiavi NetLink.","Informazioni",MessageBoxButtons.OK,MessageBoxIcon.Information);MainMenuStrip=menu;
   var root=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=1,RowCount=5,Padding=new Padding(18)};
   root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));root.RowStyles.Add(new RowStyle(SizeType.AutoSize));root.RowStyles.Add(new RowStyle(SizeType.AutoSize));root.RowStyles.Add(new RowStyle(SizeType.Percent,100));root.RowStyles.Add(new RowStyle(SizeType.AutoSize));root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
   var heading=new FlowLayoutPanel{AutoSize=true,Dock=DockStyle.Fill,WrapContents=true};headline.Text="NetLink · Controllo servizio";heading.Controls.Add(headline);heading.Controls.Add(new Label{Text="v1.2.0 · Windows x64",AutoSize=true,ForeColor=Color.DimGray,Margin=new Padding(18,9,0,0)});root.Controls.Add(heading,0,0);
   var intro=new TableLayoutPanel{AutoSize=true,Dock=DockStyle.Fill,ColumnCount=1,Padding=new Padding(0,6,0,10)};
   var help=new Label{Text="Prima analizza, poi scegli se correggere. Automatico senza ritardo non significa precedenza garantita sugli altri servizi.\r\nSe parte solo dopo l'avvio di Windows, valuta gli eventi: un avvio ritardato può essere più adatto.",AutoSize=true,MaximumSize=new Size(950,0),ForeColor=Color.FromArgb(65,82,105)};
   intro.Controls.Add(help);intro.Controls.Add(summary);root.Controls.Add(intro,0,1);
   Resize+=(s,e)=>{int width=Math.Max(240,ClientSize.Width-44);help.MaximumSize=new Size(width,0);summary.MaximumSize=new Size(width,0);status.MaximumSize=new Size(width,0);mode.Width=Math.Min(340,Math.Max(220,width-160));};
   checks.Columns.Add("Controllo",195);checks.Columns.Add("Esito",220);checks.Columns.Add("Indicazione",550);root.Controls.Add(checks,0,2);
   var controls=new FlowLayoutPanel{AutoSize=true,Dock=DockStyle.Fill,WrapContents=true,Padding=new Padding(0,10,0,5)};
   mode.Items.AddRange(new object[]{"Automatico senza ritardo","Automatico ritardato (per problemi al boot)"});mode.SelectedIndex=0;
   controls.Controls.Add(new Label{Text="Avvio da impostare:",AutoSize=true,Margin=new Padding(0,7,8,0)});controls.Controls.Add(mode);controls.Controls.Add(recovery);controls.Controls.Add(refresh);root.Controls.Add(controls,0,3);
   controls.SetFlowBreak(refresh,true);
   controls.Controls.Add(new Label{Text="Controllo dopo il boot:",AutoSize=true,Margin=new Padding(0,7,8,0)});
   startupDelay.Items.AddRange(new object[]{"Dopo 3 minuti","Dopo 5 minuti","Disattivato"});startupDelay.SelectedIndex=0;controls.Controls.Add(startupDelay);controls.Controls.Add(startup);
   var bottom=new TableLayoutPanel{AutoSize=true,Dock=DockStyle.Fill,ColumnCount=1};var buttons=new FlowLayoutPanel{AutoSize=true,Dock=DockStyle.Fill,WrapContents=true};buttons.Controls.AddRange(new Control[]{analyze,configure,start,export});bottom.Controls.Add(buttons);bottom.Controls.Add(status);root.Controls.Add(bottom,0,4);
   Controls.Add(root);Controls.Add(menu);
   tips.SetToolTip(configure,"Modifica solo il tipo di avvio e, se spuntato, il recupero del servizio NetLink. Backup obbligatorio prima della modifica. Non avvia il servizio immediatamente.");
   tips.SetToolTip(start,"Chiede a Windows di avviare il servizio riconosciuto. Non lo arresta, non cambia il tipo di avvio e non apre il Configurator.");
   tips.SetToolTip(startup,"Crea un controllo indipendente a 3 o 5 minuti dall'avvio di Windows. Non ritarda il normale avvio del servizio. Se NetLink è Manuale o Disabilitato il controllo non lo avvia.");
   tips.SetToolTip(checks,"Doppio clic su una riga per leggere l'indicazione completa. Gli eventi storici sono indizi, non una diagnosi certa della causa.");
   checks.DoubleClick+=(s,e)=>{if(checks.SelectedItems.Count>0)MessageBox.Show(this,checks.SelectedItems[0].SubItems[2].Text,checks.SelectedItems[0].Text,MessageBoxButtons.OK,MessageBoxIcon.Information);};
   worker.DoWork+=(s,e)=>{e.Result=((Func<Result>)e.Argument)();};
   worker.RunWorkerCompleted+=(s,e)=>{
    if(e.Error!=null){status.Text="Analisi non riuscita: "+Engine.Explain(e.Error);current=null;checks.Items.Clear();}
    else {var result=(Result)e.Result;current=result.Snapshot;ShowSnapshot(current);status.Text=result.Message+(result.Backup.Length>0?"\r\nBackup: "+result.Backup:"");status.ForeColor=result.Success?Color.FromArgb(25,100,70):Color.FromArgb(175,45,40);}
    EnabledActions();
   };
   analyze.Click+=(s,e)=>Run(()=>engine.Analyze());
   configure.Click+=(s,e)=>{
    var approved=current;bool delayed=mode.SelectedIndex==1,wantRecovery=recovery.Checked;
    string text="Impostare NetLink su Automatico "+(delayed?"ritardato":"senza ritardo")+"?\r\n\r\n"+(wantRecovery?"Sarà impostato anche il recupero: 10, 30, 60 secondi, poi nessun altro tentativo.\r\n":"Il recupero esistente non verrà modificato.\r\n")+"\r\nPrima viene salvato un backup dei parametri del servizio. Non vengono modificati altri servizi, dipendenze, account, configurazione o chiavi NetLink.\r\n\r\nQuesta operazione non avvia il servizio adesso e non garantisce il suo avvio al prossimo boot: va verificato dopo un riavvio.";
    if(MessageBox.Show(this,text,"Conferma modifica del servizio",MessageBoxButtons.YesNo,MessageBoxIcon.Warning,MessageBoxDefaultButton.Button2)==DialogResult.Yes)Run(()=>engine.Apply(approved,wantRecovery,false,delayed));
   };
   start.Click+=(s,e)=>{var approved=current;if(MessageBox.Show(this,"Avviare il servizio NetLink locale? I client remoti non saranno modificati. Verrà salvato prima un backup dei parametri.","Avvia servizio",MessageBoxButtons.YesNo,MessageBoxIcon.Question,MessageBoxDefaultButton.Button2)==DialogResult.Yes)Run(()=>engine.Apply(approved,false,true));};
   startup.Click+=(s,e)=>{
    var approved=current;int delay=startupDelay.SelectedIndex==0?3:startupDelay.SelectedIndex==1?5:0;
    var text=delay==0?"Disattivare il controllo dopo l'avvio?":"Verificare NetLink dopo "+delay+" minuti dall'avvio di Windows?\r\nSe è già attivo non sarà riavviato; se è fermo saranno eseguite al massimo 3 richieste di avvio, con pause di 2 minuti dopo gli errori.\r\nManuale/Disabilitato e manutenzione vengono rispettati.";
    text+="\r\n\r\nIl normale avvio Windows non cambia. Il controllo viene installato in Program Files\\NetLink Service Tools e usa un'attività SYSTEM. Non modifica configurazione, chiavi, firewall o altri servizi. Viene salvato prima un backup dei parametri.\r\n\r\nProcedere?";
    if(MessageBox.Show(this,text,"Controllo dopo l'avvio",MessageBoxButtons.YesNo,MessageBoxIcon.Warning,MessageBoxDefaultButton.Button2)==DialogResult.Yes)Run(()=>engine.ApplyStartupCheck(approved,delay));
   };
   export.Click+=(s,e)=>SaveReport();timer.Tick+=(s,e)=>{if(refresh.Checked&&!worker.IsBusy)Run(()=>engine.Analyze());};timer.Start();
   FormClosing+=(s,e)=>{if(worker.IsBusy){e.Cancel=true;MessageBox.Show(this,"Attendi il completamento dell'operazione in corso (fino a 2 minuti per verificare l'avvio).","Operazione in corso");}};
   FormClosed+=(s,e)=>{timer.Dispose();tips.Dispose();worker.Dispose();};
   Shown+=(s,e)=>{if(automatic){var area=Screen.FromControl(this).WorkingArea;Size=new Size(Math.Min(Width,area.Width-30),Math.Min(Height,area.Height-40));Location=new Point(area.Left+(area.Width-Width)/2,area.Top+(area.Height-Height)/2);Run(()=>engine.Analyze());}};
   EnabledActions();
  }
  static Button Button(string text,bool primary){return new Button{Text=text,AutoSize=true,Padding=new Padding(12,7,12,7),Margin=new Padding(0,0,9,8),FlatStyle=FlatStyle.Flat,BackColor=primary?Color.FromArgb(37,99,235):Color.White,ForeColor=primary?Color.White:Color.FromArgb(35,60,90),UseVisualStyleBackColor=false};}
  void Run(Func<Result> action){if(worker.IsBusy)return;status.Text="Operazione in corso…";worker.RunWorkerAsync(action);EnabledActions();}
  void EnabledActions(){bool idle=!worker.IsBusy;analyze.Enabled=idle;configure.Enabled=idle&&current!=null&&current.Exists&&current.Trusted&&current.ConfigKnown;start.Enabled=configure.Enabled&&current.State==1&&current.StartType!=4;startup.Enabled=configure.Enabled;startupDelay.Enabled=idle;export.Enabled=idle&&current!=null;mode.Enabled=idle;recovery.Enabled=idle;}
  public void ShowSnapshot(Snapshot snapshot){
   current=snapshot;checks.BeginUpdate();checks.Items.Clear();if(snapshot==null){summary.Text="Stato non disponibile.";checks.EndUpdate();return;}
   summary.Text=Environment.MachineName+" · Ultima analisi: "+snapshot.Checked.ToString("dd/MM/yyyy HH:mm:ss")+" · NetLink: "+(snapshot.Version.Length>0?snapshot.Version:"versione non disponibile");
   foreach(var check in Engine.Checks(snapshot)){var row=new ListViewItem(new[]{check.Title,check.State,check.Detail}){BackColor=check.Level==2?Color.FromArgb(255,229,229):check.Level==1?Color.FromArgb(255,245,213):Color.FromArgb(227,246,236),ForeColor=Color.FromArgb(37,53,70),ToolTipText=check.Detail};checks.Items.Add(row);}
   checks.EndUpdate();EnabledActions();
  }
  void SaveReport(){using(var dialog=new SaveFileDialog{Filter="Rapporto testo (*.txt)|*.txt|Dati JSON (*.json)|*.json",FileName="NetLink-servizio-"+DateTime.Now.ToString("yyyyMMdd-HHmmss"),OverwritePrompt=true}) {
   if(dialog.ShowDialog(this)!=DialogResult.OK)return;
   try {string text=Path.GetExtension(dialog.FileName).Equals(".json",StringComparison.OrdinalIgnoreCase)?new JavaScriptSerializer().Serialize(current):Report(current);File.WriteAllText(dialog.FileName,text,new UTF8Encoding(false));status.Text="Rapporto salvato: "+dialog.FileName;}catch(Exception e){MessageBox.Show(this,e.Message,"Salvataggio non riuscito",MessageBoxButtons.OK,MessageBoxIcon.Error);}
  }}
  public static string Report(Snapshot s){var b=new StringBuilder("NetLink Service Check 1.2.0\r\n");b.AppendLine("PC: "+Environment.MachineName);b.AppendLine("Analisi: "+s.Checked.ToString("o"));b.AppendLine("Ultimo avvio Windows: "+s.Boot.ToString("o"));b.AppendLine("Servizio: "+s.Name+" / "+s.Display);b.AppendLine("Comando: "+s.Image);b.AppendLine("Account: "+s.Account+" | Gruppo: "+s.Group);b.AppendLine("Versione: "+s.Version);foreach(var c in Engine.Checks(s)){b.AppendLine();b.AppendLine(c.Title+" — "+c.State);b.AppendLine(c.Detail);}b.AppendLine("\r\nEVENTI RACCOLTI (massimo 30, ricerca limitata agli ultimi 7 giorni)");foreach(var e in s.Events)b.AppendLine(e.Time.ToString("o")+" | "+e.Id+" | "+e.Message);b.AppendLine("\r\nNessuna chiave di autenticazione o password è stata raccolta. I rapporti contengono percorsi e nomi locali: controllali prima di condividerli.");return b.ToString();}
 }
 static class Program {
  [STAThread] static int Main(string[] args){
   Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
   if(args.Length==2&&args[0]=="--report") {try{File.WriteAllText(args[1],MainForm.Report(new WindowsHost().Read()),new UTF8Encoding(false));return 0;}catch{return 1;}}
   if(args.Length!=0){MessageBox.Show("Parametro non riconosciuto.","NetLink");return 2;}
   Application.Run(new MainForm());return 0;
  }
 }
}
