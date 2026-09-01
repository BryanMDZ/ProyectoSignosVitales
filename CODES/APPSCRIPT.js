const HOJA_PACIENTES = "Pacientes";
const HOJA_DATOS = "Datos";
const HOJA_REPORTE = "Reporte";

function doGet(e) {
  const ss = SpreadsheetApp.getActiveSpreadsheet();

  // REGISTRO DE PACIENTE
  if (e.parameter.registrarPaciente) {
    return ContentService.createTextOutput(
      registrarPaciente(
        e.parameter.nombre,
        e.parameter.edad,
        e.parameter.sexo,
        e.parameter.obs
      )
    );
  }

  // REPORTE
  if (e.parameter.reporte) {
    const resultado = generarReportePaciente(
      e.parameter.id,
      e.parameter.ini,
      e.parameter.fin,
      e.parameter.horaIni,
      e.parameter.horaFin
    );
    return ContentService
      .createTextOutput(JSON.stringify(resultado))
      .setMimeType(ContentService.MimeType.JSON);
  }

  // GUARDAR DATOS BIOM�DICOS
  if (e.parameter.id) {
    return ContentService.createTextOutput(
      guardarDatosPaciente(
        e.parameter.id,
        e.parameter.bpm,
        e.parameter.spo2,
        e.parameter.temp
      )
    );
  }
  // LISTAR PACIENTES PARA REPORTE
  if (e.parameter.listarPacientes) {
    return listarPacientes();
  }
  // MOSTRAR PACIENTE ACTIVO
  if (e.parameter.pacienteActivo) {
    return obtenerPacienteActivo();
  }
  return ContentService.createTextOutput("Sistema activo");
  
}

// =========================================
// REGISTRO DE PACIENTES
// =========================================

function registrarPaciente(nombre, edad, sexo, obs) {
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  let hoja = ss.getSheetByName(HOJA_PACIENTES);

  if (!hoja) {
    hoja = ss.insertSheet(HOJA_PACIENTES);
    hoja.appendRow(["ID", "Nombre", "Edad", "Sexo", "Observaciones"]);
  }

  const nuevoID = generarNuevoID(hoja);

  hoja.appendRow([
    nuevoID,
    nombre,
    edad,
    sexo,
    obs || ""
  ]);

  return "Paciente registrado con ID: " + nuevoID;
}

function generarNuevoID(hoja) {
  const lastRow = hoja.getLastRow();

  if (lastRow <= 1) return "001";

  const ultimoID = hoja.getRange(lastRow, 1).getValue();
  const nuevoNumero = parseInt(ultimoID) + 1;

  return String(nuevoNumero).padStart(3, '0');
}

// =========================================
// GUARDAR DATOS POR PACIENTE
// =========================================

function guardarDatosPaciente(id, bpm, spo2, temp) {
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  let hoja = ss.getSheetByName(HOJA_DATOS);

  if (!hoja) {
  hoja = ss.insertSheet(HOJA_DATOS);
  }

  // Verificar encabezado
  if (hoja.getLastRow() === 0) {
    hoja.appendRow([
      "ID",
      "Fecha",
      "Hora",
      "BPM",
      "SpO2",
      "Temp"
    ]);
  }

  const now = new Date();

  const fecha = Utilities.formatDate(
    now,
    Session.getScriptTimeZone(),
    "yyyy-MM-dd"
  );

  const hora = Utilities.formatDate(
    now,
    Session.getScriptTimeZone(),
    "HH:mm:ss"
  );

  hoja.appendRow([
    id,
    fecha,
    hora,
    bpm || 0,
    spo2 || 0,
    temp || 0
  ]);

  return "Datos guardados correctamente";
}

// =========================================
// REPORTES PERSONALIZADOS
// =========================================
function listarPacientes() {
  const hoja = SpreadsheetApp.getActiveSpreadsheet().getSheetByName("Pacientes");
  const datos = hoja.getDataRange().getValues();

  const pacientes = [];

  for (let i = 1; i < datos.length; i++) {
    if (datos[i][0] && datos[i][1]) {
      pacientes.push({
        id: String(datos[i][0]),
        nombre: String(datos[i][1])
      });
    }
  }

  return ContentService
    .createTextOutput(JSON.stringify(pacientes))
    .setMimeType(ContentService.MimeType.JSON);
}

function obtenerInfoPaciente(idBuscado) {
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  const hojaPacientes = ss.getSheetByName(HOJA_PACIENTES);

  if (!hojaPacientes) {
    return {
      id: idBuscado,
      nombre: "Desconocido",
      edad: "",
      sexo: "",
      obs: ""
    };
  }

  const datos = hojaPacientes.getDataRange().getValues();

  for (let i = 1; i < datos.length; i++) {
    const id = String(datos[i][0]).padStart(3, "0");

    if (id === String(idBuscado).padStart(3, "0")) {
      return {
        id: id,
        nombre: datos[i][1] || "Desconocido",
        edad: datos[i][2] || "",
        sexo: datos[i][3] || "",
        obs: datos[i][4] || ""
      };
    }
  }

  return {
    id: idBuscado,
    nombre: "Desconocido",
    edad: "",
    sexo: "",
    obs: ""
  };
}

function generarReportePaciente(id, fechaInicio, fechaFin, horaInicio, horaFin) {

  const ss = SpreadsheetApp.getActiveSpreadsheet();
  const datos = ss.getSheetByName(HOJA_DATOS);

  if (!datos) {
    return {
      ok: false,
      mensaje: "No existe hoja de datos",
      pdfUrl: ""
    };
  }

  const pacienteIDBuscado = String(id).padStart(3, "0");
  const infoPaciente = obtenerInfoPaciente(pacienteIDBuscado);

  let reporte = ss.getSheetByName(HOJA_REPORTE);

  if (!reporte) {
    reporte = ss.insertSheet(HOJA_REPORTE);
  } else {
    reporte.clear();

    const charts = reporte.getCharts();
    charts.forEach(chart => reporte.removeChart(chart));
  }

  const data = datos.getDataRange().getValues();
  const filtrados = [];

  for (let i = 1; i < data.length; i++) {

    const pacienteID = String(data[i][0]).padStart(3, "0");

    const fecha = Utilities.formatDate(
      new Date(data[i][1]),
      Session.getScriptTimeZone(),
      "yyyy-MM-dd"
    );

    const hora = Utilities.formatDate(
      new Date(data[i][2]),
      Session.getScriptTimeZone(),
      "HH:mm:ss"
    );

    if (
      pacienteID === pacienteIDBuscado &&
      fecha >= fechaInicio &&
      fecha <= fechaFin &&
      hora >= horaInicio &&
      hora <= horaFin
    ) {
      // Se elimina ID y Timestamp de la tabla principal
      filtrados.push([
        fecha,
        hora,
        data[i][3],
        data[i][4],
        data[i][5]
      ]);
    }
  }

  if (filtrados.length === 0) {
    return {
      ok: false,
      mensaje: "No hay datos para ese paciente en ese rango",
      pdfUrl: ""
    };
  }

  // ==========================
  // ENCABEZADO DEL REPORTE
  // ==========================

  reporte.getRange("A1:H1").merge();
  reporte.getRange("A1").setValue("REPORTE DE MONITOREO DE SIGNOS VITALES");
  reporte.getRange("A1")
    .setFontSize(14)
    .setFontWeight("bold")
    .setHorizontalAlignment("center")
    .setBackground("#1976D2")
    .setFontColor("white");

  reporte.getRange("A3").setValue("Paciente:");
  reporte.getRange("B3").setValue(`${infoPaciente.id} - ${infoPaciente.nombre}`);

  reporte.getRange("D3").setValue("Edad:");
  reporte.getRange("E3").setValue(infoPaciente.edad);

  reporte.getRange("F3").setValue("Sexo:");
  reporte.getRange("G3").setValue(infoPaciente.sexo);

  reporte.getRange("A4").setValue("Periodo:");
  reporte.getRange("B4").setValue(`${fechaInicio} ${horaInicio}  a  ${fechaFin} ${horaFin}`);

  reporte.getRange("A5").setValue("Generado:");
  reporte.getRange("B5").setValue(
    Utilities.formatDate(
      new Date(),
      Session.getScriptTimeZone(),
      "yyyy-MM-dd HH:mm:ss"
    )
  );

  reporte.getRange("A6").setValue("Observaciones:");
  reporte.getRange("B6:H6").merge();
  reporte.getRange("B6").setValue(infoPaciente.obs || "Sin observaciones");

  reporte.getRange("A3:H6").setBorder(true, true, true, true, true, true);

  reporte.getRange("A3:A6").setFontWeight("bold");
  reporte.getRange("D3").setFontWeight("bold");
  reporte.getRange("F3").setFontWeight("bold");

  // ==========================
  // TABLA DE MEDICIONES
  // ==========================

  const filaEncabezadoTabla = 8;
  const filaInicioDatos = 9;

  reporte.getRange(filaEncabezadoTabla, 1, 1, 5).setValues([[
    "Fecha",
    "Hora",
    "BPM",
    "SpO2",
    "Temp"
  ]]);

  reporte.getRange(
    filaInicioDatos,
    1,
    filtrados.length,
    filtrados[0].length
  ).setValues(filtrados);

  const lastRow = reporte.getLastRow();

  reporte.getRange(`A${filaEncabezadoTabla}:E${filaEncabezadoTabla}`)
    .setFontWeight("bold")
    .setBackground("#BBDEFB")
    .setHorizontalAlignment("center");

  reporte.getRange(`A${filaInicioDatos}:E${lastRow}`)
    .setBorder(true, true, true, true, true, true);

  reporte.getRange(`A${filaInicioDatos}:A${lastRow}`).setHorizontalAlignment("center");
  reporte.getRange(`B${filaInicioDatos}:B${lastRow}`).setHorizontalAlignment("center");
  reporte.getRange(`C${filaInicioDatos}:D${lastRow}`).setHorizontalAlignment("center");
  reporte.getRange(`E${filaInicioDatos}:E${lastRow}`).setHorizontalAlignment("center");

  // Formato num�rico
  reporte.getRange(`C${filaInicioDatos}:C${lastRow}`).setNumberFormat("0");
  reporte.getRange(`D${filaInicioDatos}:D${lastRow}`).setNumberFormat("0");
  reporte.getRange(`E${filaInicioDatos}:E${lastRow}`).setNumberFormat("0.00");

  // ==========================
  // M�TRICAS
  // ==========================

  reporte.getRange("G8").setValue("M�trica");
  reporte.getRange("H8").setValue("Valor");

  reporte.getRange("G8:H8")
    .setFontWeight("bold")
    .setBackground("#1976D2")
    .setFontColor("white")
    .setHorizontalAlignment("center");

  // BPM
  reporte.getRange("G9").setValue("Promedio BPM");
  reporte.getRange("H9").setFormula(`=ROUND(AVERAGE(C${filaInicioDatos}:C${lastRow}),0)`);

  reporte.getRange("G10").setValue("M�ximo BPM");
  reporte.getRange("H10").setFormula(`=MAX(C${filaInicioDatos}:C${lastRow})`);

  reporte.getRange("G11").setValue("M�nimo BPM");
  reporte.getRange("H11").setFormula(`=MIN(C${filaInicioDatos}:C${lastRow})`);

  // SpO2
  reporte.getRange("G13").setValue("Promedio SpO2");
  reporte.getRange("H13").setFormula(`=ROUND(AVERAGE(D${filaInicioDatos}:D${lastRow}),0)`);

  reporte.getRange("G14").setValue("M�ximo SpO2");
  reporte.getRange("H14").setFormula(`=MAX(D${filaInicioDatos}:D${lastRow})`);

  reporte.getRange("G15").setValue("M�nimo SpO2");
  reporte.getRange("H15").setFormula(`=MIN(D${filaInicioDatos}:D${lastRow})`);

  // Temperatura
  reporte.getRange("G17").setValue("Promedio Temp");
  reporte.getRange("H17").setFormula(`=TRUNC(AVERAGE(E${filaInicioDatos}:E${lastRow}),2)`);

  reporte.getRange("G18").setValue("M�xima Temp");
  reporte.getRange("H18").setFormula(`=TRUNC(MAX(E${filaInicioDatos}:E${lastRow}),2)`);

  reporte.getRange("G19").setValue("M�nima Temp");
  reporte.getRange("H19").setFormula(`=TRUNC(MIN(E${filaInicioDatos}:E${lastRow}),2)`);

  reporte.getRange("G8:H19")
    .setBorder(true, true, true, true, true, true);

  reporte.getRange("H9:H19").setHorizontalAlignment("center");

  // ==========================
  // AJUSTE DE COLUMNAS
  // ==========================

  reporte.setColumnWidth(1, 100); // Fecha
  reporte.setColumnWidth(2, 90);  // Hora
  reporte.setColumnWidth(3, 65);  // BPM
  reporte.setColumnWidth(4, 65);  // SpO2
  reporte.setColumnWidth(5, 65);  // Temp

  // M�tricas m�s amplias
  reporte.setColumnWidth(7, 150);
  reporte.setColumnWidth(8, 90);

  reporte.setFrozenRows(8);

  SpreadsheetApp.flush();

  // ==========================
  // GR�FICAS
  // ==========================

  const ultimaFilaContenido = Math.max(lastRow, 19);

  // Deja espacio entre tablas y gr�ficas
  const filaGrafica1 = ultimaFilaContenido + 4;
  const filaGrafica2 = filaGrafica1 + 20;
  const filaGrafica3 = filaGrafica2 + 20;

  const chartBPM = reporte.newChart()
    .setChartType(Charts.ChartType.LINE)
    .addRange(reporte.getRange(`B${filaEncabezadoTabla}:B${lastRow}`))
    .addRange(reporte.getRange(`C${filaEncabezadoTabla}:C${lastRow}`))
    .setPosition(filaGrafica1, 1, 0, 0)
    .setOption("title", "Frecuencia Card�aca (BPM)")
    .setOption("legend", { position: "right" })
    .setOption("vAxis", {viewWindow: {min: 40,max: 120},ticks: [40, 60, 80, 100, 120]})
    .build();

  reporte.insertChart(chartBPM);

  const chartSpO2 = reporte.newChart()
    .setChartType(Charts.ChartType.LINE)
    .addRange(reporte.getRange(`B${filaEncabezadoTabla}:B${lastRow}`))
    .addRange(reporte.getRange(`D${filaEncabezadoTabla}:D${lastRow}`))
    .setPosition(filaGrafica2, 1, 0, 0)
    .setOption("title", "Saturaci�n de Ox�geno (SpO2)")
    .setOption("legend", { position: "right" })
    .setOption("vAxis", {viewWindow: {min: 70,max: 100},ticks: [70, 80, 90, 100]})
    .build();

  reporte.insertChart(chartSpO2);

  const chartTemp = reporte.newChart()
    .setChartType(Charts.ChartType.LINE)
    .addRange(reporte.getRange(`B${filaEncabezadoTabla}:B${lastRow}`))
    .addRange(reporte.getRange(`E${filaEncabezadoTabla}:E${lastRow}`))
    .setPosition(filaGrafica3, 1, 0, 0)
    .setOption("title", "Temperatura Corporal")
    .setOption("legend", { position: "right" })
    .setOption("vAxis", {viewWindow: {min: 30,max: 50},ticks: [30, 35, 40, 45]})
    .build();

  reporte.insertChart(chartTemp);

  SpreadsheetApp.flush();

  // ==========================
  // EXPORTAR PDF EN CARTA VERTICAL
  // ==========================

  const url = ss.getUrl().replace(/edit$/, "") +
    "export?format=pdf" +
    "&gid=" + reporte.getSheetId() +
    "&portrait=true" +
    "&size=letter" +
    "&fitw=true" +
    "&sheetnames=false" +
    "&printtitle=false" +
    "&pagenumbers=false" +
    "&gridlines=false" +
    "&fzr=false";

  const response = UrlFetchApp.fetch(url, {
    headers: {
      Authorization: "Bearer " + ScriptApp.getOAuthToken()
    }
  });

  const nombrePDF =
    "Reporte_Paciente_" +
    pacienteIDBuscado +
    "_" +
    fechaInicio +
    "_a_" +
    fechaFin +
    ".pdf";

  const blob = response.getBlob().setName(nombrePDF);
  const archivoPDF = DriveApp.createFile(blob);

  archivoPDF.setSharing(
    DriveApp.Access.ANYONE_WITH_LINK,
    DriveApp.Permission.VIEW
  );

  const pdfUrl =
    "https://drive.google.com/uc?export=download&id=" +
    archivoPDF.getId();

  return {
    ok: true,
    mensaje: "Reporte generado correctamente",
    pdfUrl: pdfUrl
  };
}

function diagnosticarDatosPaciente() {

  const ss = SpreadsheetApp.getActiveSpreadsheet();
  const hoja = ss.getSheetByName(HOJA_DATOS);

  if (!hoja) {
    Logger.log("No existe hoja de datos");
    return;
  }

  const data = hoja.getDataRange().getValues();

  Logger.log("=== DIAGN�STICO DE DATOS ===");

  for (let i = 1; i < Math.min(data.length, 15); i++) {

    const id = data[i][0];
    const fecha = data[i][1];
    const hora = data[i][2];
    const bpm = data[i][3];
    const spo2 = data[i][4];
    const temp = data[i][5];

    Logger.log("Fila: " + (i + 1));

    Logger.log("ID valor: " + id);
    Logger.log("ID tipo: " + typeof id);

    Logger.log("Fecha valor: " + fecha);
    Logger.log("Fecha tipo: " + typeof fecha);
    Logger.log("Fecha instanceof Date: " + (fecha instanceof Date));

    Logger.log("Hora valor: " + hora);
    Logger.log("Hora tipo: " + typeof hora);

    Logger.log("BPM: " + bpm);
    Logger.log("SpO2: " + spo2);
    Logger.log("Temp: " + temp);

    Logger.log("---------------------------");
  }
}

function obtenerPacienteActivo() {
  const ss = SpreadsheetApp.getActiveSpreadsheet();

  const hojaDatos = ss.getSheetByName("Datos");
  const hojaPacientes = ss.getSheetByName("Pacientes");

  const datos = hojaDatos.getDataRange().getValues();
  const pacientes = hojaPacientes.getDataRange().getValues();

  if (datos.length < 2) {
    return ContentService
      .createTextOutput(JSON.stringify({ id: "--", nombre: "Sin datos" }))
      .setMimeType(ContentService.MimeType.JSON);
  }

  const ultimoID = String(datos[datos.length - 1][0]);

  let nombrePaciente = "Desconocido";

  for (let i = 1; i < pacientes.length; i++) {
    if (String(pacientes[i][0]) === ultimoID) {
      nombrePaciente = pacientes[i][1];
      break;
    }
  }

  return ContentService
    .createTextOutput(JSON.stringify({
      id: ultimoID,
      nombre: nombrePaciente
    }))
    .setMimeType(ContentService.MimeType.JSON);
}
