// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 prof. Ing. Raffaele Mele
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;

namespace NetLinkServiceCheck {
 public sealed class Evidence { public int Id; public DateTime Time; public string Message; }
 public sealed class Check { public string Title,State,Detail; public int Level; public Check(string title,int level,string state,string detail){Title=title;Level=level;State=state;Detail=detail;} }
 public sealed class Snapshot {
  public DateTime Checked=DateTime.Now,Boot=DateTime.MinValue;
  public bool Exists,Trusted,Delayed,Recovery,ConfigKnown=true;
  public string Name="VeyonSchoolService",Display="NetLink Service",Image="",Executable="",Account="",Version="",Group="",IdentityReason="",ReadError="";
  public uint StartType,State,Pid,ExitCode,SpecificExitCode,TriggerCount;
  public int StartupDelay=-1;public bool StartupTask;public string StartupInfo="Controllo dopo l'avvio non ancora verificato.";
  public string[] Dependencies=new string[0],DependencyProblems=new string[0];
  public string[] Warnings=new string[0]; public Evidence[] Events=new Evidence[0];
  public string Fingerprint(){return String.Join("|",new[]{Name,Image,Account,Version,Group,StartType.ToString(),Delayed.ToString(),Recovery.ToString(),String.Join(";",Dependencies),StartupDelay.ToString(),StartupTask.ToString()});}
 }
 public interface IHost {
  Snapshot Read(); string Backup(Snapshot snapshot);
  void SetAutomatic(); void SetDelay(bool delayed); void SetRecovery(); void Start();
 }
 public interface IStartupCheck { void ConfigureStartupCheck(int minutes); }
 public sealed class Result {public Snapshot Snapshot;public string Message="",Backup="";public bool Success;}
 public sealed class Engine {
  readonly IHost host;public Engine(IHost host){this.host=host;}
  public Result Analyze(){return new Result{Snapshot=host.Read(),Success=true,Message="Analisi in sola lettura. Nessuna impostazione modificata."};}
  public Result ApplyStartupCheck(Snapshot approved,int minutes) {
   var result=new Result();
   try {
    if(minutes!=0&&minutes!=3&&minutes!=5)throw new InvalidOperationException("Scegli 3 minuti, 5 minuti o Disattivato.");
    var before=host.Read();var startup=host as IStartupCheck;
    if(startup==null||approved==null||!before.Exists||!before.Trusted||!before.ConfigKnown||before.Fingerprint()!=approved.Fingerprint())throw new InvalidOperationException("Parametri cambiati o servizio non riconosciuto: analizza di nuovo prima di configurare il controllo.");
    result.Backup=host.Backup(before);startup.ConfigureStartupCheck(minutes);
    var after=host.Read();result.Snapshot=after;
    if(!after.Trusted||after.Image!=before.Image||after.Account!=before.Account||after.StartType!=before.StartType||after.Delayed!=before.Delayed||after.Recovery!=before.Recovery||!after.Dependencies.SequenceEqual(before.Dependencies)||after.StartupDelay!=minutes||after.StartupTask!=(minutes!=0))throw new InvalidOperationException("Verifica finale del controllo non riuscita. Consulta il rapporto; nessun ripristino automatico alla cieca.");
    result.Success=true;result.Message=minutes==0?"Controllo dopo l'avvio disattivato.":"Controllo dopo "+minutes+" minuti configurato e verificato. Il servizio mantiene il tipo di avvio esistente; il controllo non interviene su Manuale/Disabilitato.";
   }catch(Exception error){result.Message="Operazione non completata: "+Explain(error);try{result.Snapshot=host.Read();}catch{result.Snapshot=approved;}}
   return result;
  }
  public Result Apply(Snapshot approved,bool recovery,bool start,bool delayed=false) {
   var result=new Result();
   try {
    var before=host.Read();
    if(approved==null || !before.Exists || !before.ConfigKnown || !before.Trusted) throw new InvalidOperationException("Modifica rifiutata: il servizio NetLink non è identificato con certezza o i parametri non sono leggibili.");
    if(before.Fingerprint()!=approved.Fingerprint()) throw new InvalidOperationException("I parametri del servizio sono cambiati dopo l'analisi. Analizza di nuovo; nessuna modifica eseguita.");
    if(start && before.State==4){result.Success=true;result.Message="Il servizio è già in esecuzione.";result.Snapshot=before;return result;}
    if(start && before.StartType==4) throw new InvalidOperationException("Il servizio è disabilitato. Imposta prima l'avvio automatico.");
    if(start && before.State!=1) throw new InvalidOperationException("Il servizio è in transizione o sospeso. Attendi e ripeti l'analisi; non verrà forzato un arresto.");
    result.Backup=host.Backup(before); // Mandatory before any mutation.
    if(start) host.Start();
    else {host.SetAutomatic();host.SetDelay(delayed);if(recovery)host.SetRecovery();}
    var after=host.Read();result.Snapshot=after;
    if(!after.Exists || !after.ConfigKnown || !after.Trusted || after.Image!=before.Image || after.Account!=before.Account || after.Group!=before.Group || !after.Dependencies.SequenceEqual(before.Dependencies))
     throw new InvalidOperationException("Verifica finale non riuscita: identità o parametri estranei all'operazione sono cambiati. Consulta il rapporto; non verranno fatte altre correzioni.");
    if(start && after.State!=4) throw new InvalidOperationException("Il servizio non ha raggiunto lo stato In esecuzione. Consulta errori Windows, dipendenze ed eventi nel rapporto.");
    if(!start && (after.StartType!=2 || after.Delayed!=delayed || (recovery && !after.Recovery))) throw new InvalidOperationException("Windows non conferma tutti i parametri richiesti. La modifica potrebbe essere parziale; consulta il rapporto.");
    result.Success=true;result.Message=start?"Servizio avviato e stato verificato.":"Avvio automatico "+(delayed?"ritardato":"senza ritardo")+" verificato. L'effettivo avvio al prossimo riavvio va ancora controllato.";
   } catch(Exception error) {
    result.Message="Operazione non completata: "+Explain(error)+"\r\nI passaggi già applicati non vengono annullati alla cieca: premi Analizza per vedere lo stato effettivo.";
    try{result.Snapshot=host.Read();}catch{result.Snapshot=approved;}
   }
   return result;
  }
  public static string Explain(Exception error) {
   var native=error as Win32Exception;if(native==null)return error.Message;
   string advice="";
   switch(native.NativeErrorCode){
    case 5:advice="Avvia come amministratore. Un criterio aziendale o i permessi del servizio possono impedire la modifica; non vengono aggirati.";break;
    case 1060:advice="Servizio non registrato: ripara l'installazione NetLink conservando configurazione e chiavi.";break;
    case 1072:advice="Servizio contrassegnato per l'eliminazione. Chiudi gli strumenti di gestione e, se necessario, riavvia Windows.";break;
    case 1077:advice="Windows non ha tentato l'avvio del servizio dopo l'ultimo riavvio. Controlla anzitutto il tipo di avvio; non è un errore di autenticazione NetLink.";break;
    case 1058:advice="Servizio disabilitato: correggi il tipo di avvio prima di avviarlo.";break;
    case 1053:case 1460:advice="Tempo di avvio scaduto. Controlla gli eventi e le dipendenze; non viene aumentato il timeout globale di Windows.";break;
    case 1068:advice="Una dipendenza non si è avviata. Controlla l'elenco delle dipendenze; non vengono modificate automaticamente.";break;
    case 1069:advice="Accesso dell'account del servizio non riuscito. Verifica account, diritti e credenziali; la password del servizio non viene cambiata.";break;
    case 2:case 3:advice="Percorso o eseguibile del servizio non trovato. Ripara l'installazione ufficiale di NetLink.";break;
    case 193:advice="Il file non è un eseguibile Windows compatibile o il percorso è errato.";break;
   }
   return "Errore Windows "+native.NativeErrorCode+": "+native.Message+" "+advice;
  }
  public static List<Check> Checks(Snapshot s) {
   var list=new List<Check>();
   list.Add(new Check("Registrazione",!s.ConfigKnown?2:s.Exists?0:2,!s.ConfigKnown?"Non leggibile":s.Exists?"Presente":"Assente",s.ReadError.Length>0?s.ReadError:"Nome tecnico del servizio NetLink: VeyonSchoolService. Il nome visualizzato è NetLink Service."));
   if(!s.Exists || !s.ConfigKnown)return list;
   list.Add(new Check("Identità NetLink",s.Trusted?0:2,s.Trusted?"Riconosciuta":"Correzioni bloccate",s.IdentityReason));
   string state=s.State==4?"In esecuzione":s.State==1?"Arrestato":s.State==2?"Avvio in corso":s.State==3?"Arresto in corso":"Stato "+s.State;
   list.Add(new Check("Stato del servizio",s.State==4?0:1,state,s.State==1&&s.ExitCode==0?"Un arresto senza errori può essere intenzionale: da solo non dimostra un mancato avvio automatico.":"PID "+s.Pid+" · codice Windows "+s.ExitCode+" · codice specifico "+s.SpecificExitCode+(s.ExitCode>0?"\r\n"+Explain(new Win32Exception(unchecked((int)s.ExitCode))):"")));
   list.Add(new Check("Avvio Windows",s.StartType==2&&!s.Delayed?0:1,s.StartType==2?(s.Delayed?"Automatico ritardato":"Automatico senza ritardo"):s.StartType==3?"Manuale":s.StartType==4?"Disabilitato":"Tipo "+s.StartType,"Senza ritardo evita la coda dei servizi automatici ritardati, ma non garantisce precedenza sugli altri servizi automatici. Dipendenze e ordine di Windows restano invariati."));
   list.Add(new Check("Controllo dopo il boot",s.StartupDelay<0||(!s.StartupTask&&s.StartupDelay>0)?1:0,s.StartupDelay<0?"Non verificato":s.StartupTask?"Dopo "+s.StartupDelay+" minuti":s.StartupDelay==0?"Disattivato":"Attività assente",s.StartupInfo+" Non ritarda l'avvio normale. Massimo 3 richieste di avvio, pausa di 2 minuti dopo un errore; rispetta Manuale/Disabilitato e la manutenzione."));
   if(s.StartType==2 && s.State==1)list.Add(new Check("Automatico ma fermo",1,"Cercare gli eventi di avvio","Il tipo di avvio è già Automatico. Cambiarlo di nuovo non dimostra di aver risolto il problema: confronta timeout, arresti e cambi di configurazione nel registro. Se l'avvio manuale riesce, valuta anche il ritardo all'avvio."));
   if(s.Events.Any(e=>e.Time>=s.Boot && (e.Id==7009 || e.Id==7011 || e.Id==7022)))list.Add(new Check("Timeout nell'avvio attuale",1,"Possibile problema di tempistica","Windows ha registrato un timeout pertinente. Prova l'automatico ritardato su questo PC e controlla dopo un riavvio. Non è una prova della causa hardware o software e non va confuso con una priorità CPU."));
   if(s.Events.Any(e=>e.Id==7040))list.Add(new Check("Tipo di avvio modificato",1,"Evento 7040 presente","Windows ha registrato un cambiamento del tipo di avvio. Il rapporto non identifica chi l'ha effettuato: potrebbe essere un installer, una scelta manuale, un criterio o altro software. Non viene attribuito automaticamente all'antivirus."));
   list.Add(new Check("Recupero dopo errore",s.Recovery?0:1,s.Recovery?"Profilo NetLink attivo":"Diverso dal profilo NetLink","Profilo proposto: riavvio dopo 10, 30 e 60 secondi, poi nessun ulteriore tentativo; contatore azzerato dopo 24 ore. Non riavvia il PC e non esegue comandi esterni."));
   list.Add(new Check("Dipendenze",s.DependencyProblems.Length==0?0:1,s.Dependencies.Length==0?"Nessuna dichiarata":String.Join(", ",s.Dependencies),s.DependencyProblems.Length==0?"Nessun problema rilevato. Una dipendenza manuale può comunque essere avviata da Windows su richiesta.":String.Join("; ",s.DependencyProblems)));
   if(s.TriggerCount>0)list.Add(new Check("Trigger del servizio",1,s.TriggerCount.ToString(),"Sono presenti condizioni di avvio/arresto del servizio. Non vengono modificate: lo stato Arrestato va valutato anche rispetto ai trigger."));
   foreach(var warning in s.Warnings)list.Add(new Check("Limite della verifica",1,"Da verificare",warning));
   if(s.Events.Length==0)list.Add(new Check("Eventi Windows",1,"Nessuna evidenza raccolta","Nessun evento pertinente negli ultimi 7 giorni, oppure registro non leggibile. Assenza di eventi non significa assenza di problemi."));
   else foreach(var e in s.Events.Take(12))list.Add(new Check("Evento "+e.Id, e.Id==7036?0:1,e.Time.ToString("dd/MM HH:mm"),e.Message+(e.Time<s.Boot?" [Precedente all'avvio attuale]":"")));
   list.Add(new Check("Cosa non viene certificato",0,"Solo servizio locale","In esecuzione non prova che video, autenticazione, discovery e controllo remoto funzionino. Non vengono aperte porte aggiuntive."));
   return list;
  }
 }
}
