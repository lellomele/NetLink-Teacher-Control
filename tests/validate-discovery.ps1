param([string]$Fixtures)
$ErrorActionPreference='Stop'
if(!$Fixtures){throw 'Specify the directory of generated SOAP XML fixtures'}
$schemas=[Xml.Schema.XmlSchemaSet]::new()
$schemas.XmlResolver=$null
foreach($file in @('xml.xsd','ws-addr.xsd','soap-envelope.xsd','wsdd.xsd')){
    $path=Join-Path $PSScriptRoot "schemas/$file"
    [xml]$schemaXml=Get-Content -Raw -LiteralPath $path
    $schemas.Add($schemaXml.DocumentElement.GetAttribute('targetNamespace'),$path) | Out-Null
}
$schemas.Compile()
$errors=[Collections.Generic.List[string]]::new()
$settings=[Xml.XmlReaderSettings]::new()
$settings.Schemas=$schemas;$settings.ValidationType=[Xml.ValidationType]::Schema
$settings.XmlResolver=$null;$settings.DtdProcessing=[Xml.DtdProcessing]::Prohibit
$settings.add_ValidationEventHandler({param($sender,$eventArgs) $errors.Add($eventArgs.Message)})
$files=Get-ChildItem -LiteralPath $Fixtures -File -Filter '*.xml'
if($files.Count-ne6){throw 'Expected six SOAP packet fixtures'}
foreach($file in $files){
    $reader=[Xml.XmlReader]::Create($file.FullName,$settings)
    try{while($reader.Read()){} }finally{$reader.Dispose()}
    if($errors.Count){throw ($file.Name+': '+($errors -join '; '))}
    Write-Output "Schema PASS: $($file.Name)"
}
