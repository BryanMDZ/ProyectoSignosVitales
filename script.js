let reportePDFUrl = "";
const API_URL = "https://script.google.com/macros/s/AKfycbyD3u_MZi6mx24h-mzM3dxZ4kcg4bdqDkkVf_SgsM_41IvpiL4urzslq2WQP2qo7uT2/exec";

async function registrarPaciente() {
  const nombre = document.getElementById("nombre").value;
  const edad = document.getElementById("edad").value;
  const sexo = document.getElementById("sexo").value;
  const obs = document.getElementById("obs").value;

  const url = `${API_URL}?registrarPaciente=1&nombre=${encodeURIComponent(nombre)}&edad=${edad}&sexo=${sexo}&obs=${encodeURIComponent(obs)}`;

  const res = await fetch(url);
  const texto = await res.text();

  document.getElementById("registroResultado").innerText = texto;
}

async function cargarPacientes() {
  try {
    const res = await fetch(`${API_URL}?listarPacientes=1`);

    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    const pacientes = await res.json();

    const select = document.getElementById("pacienteID");

    select.innerHTML = '<option value="">Seleccione un paciente</option>';

    pacientes.forEach(paciente => {
      const option = document.createElement("option");
      option.value = paciente.id;
      option.textContent = `${paciente.id} - ${paciente.nombre}`;
      select.appendChild(option);
    });

  } catch (error) {
    console.error("Error cargando pacientes:", error);

    document.getElementById("reporteResultado").innerText =
      "Error cargando lista de pacientes.";
  }
}

async function cargarPacienteActivo() {
  try {
    const res = await fetch(`${API_URL}?pacienteActivo=1`);

    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    const paciente = await res.json();

    document.getElementById("MostrarPacienteID").innerText =
      `${paciente.id} - ${paciente.nombre}`;

  } catch (error) {
    console.error("Error cargando paciente activo:", error);

    document.getElementById("MostrarPacienteID").innerText =
      "Paciente activo: Error";
  }
}

async function generarReporte() {
  const id = document.getElementById("pacienteID").value.trim();
  const iniRaw = document.getElementById("fechaInicio").value;
  const finRaw = document.getElementById("fechaFin").value;
  const horaIniRaw = document.getElementById("horaInicio").value;
  const horaFinRaw = document.getElementById("horaFin").value;

  const resultado = document.getElementById("reporteResultado");
  const botonPDF = document.getElementById("btnDescargarPDF");

  botonPDF.style.display = "none";
  reportePDFUrl = "";

  if (!id || !iniRaw || !finRaw || !horaIniRaw || !horaFinRaw) {
    resultado.innerText = "Completa todos los campos para generar el reporte.";
    return;
  }

  function formatearFecha(fechaInput) {
    const fecha = new Date(fechaInput + "T00:00:00");

    return fecha.getFullYear() + "-" +
      String(fecha.getMonth() + 1).padStart(2, "0") + "-" +
      String(fecha.getDate()).padStart(2, "0");
  }

  function formatearHora(horaInput) {
    const [horas, minutos] = horaInput.split(":");

    return `${horas.padStart(2, "0")}:${minutos.padStart(2, "0")}:00`;
  }

  const ini = formatearFecha(iniRaw);
  const fin = formatearFecha(finRaw);
  const horaIni = formatearHora(horaIniRaw);
  const horaFin = formatearHora(horaFinRaw);

  const url =
    `${API_URL}?reporte=1` +
    `&id=${encodeURIComponent(id)}` +
    `&ini=${ini}` +
    `&fin=${fin}` +
    `&horaIni=${horaIni}` +
    `&horaFin=${horaFin}`;

  resultado.innerText = "Generando reporte...";

  try {
    const res = await fetch(url);

    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    const data = await res.json();

    resultado.innerText = data.mensaje;

    if (data.ok && data.pdfUrl) {
      reportePDFUrl = data.pdfUrl;
      botonPDF.style.display = "block";
    }

  } catch (error) {
    console.error("Error generando reporte:", error);
    resultado.innerText = `Error al generar reporte: ${error.message}`;
  }
}

async function actualizarMonitoreo() {
  try {
    const res = await fetch("http://192.168.0.242/json");
    const data = await res.json();

    document.getElementById("bpm").innerText = data.bpm;
    document.getElementById("spo2").innerText = data.spo2;
    document.getElementById("temp").innerText = data.temp;
  } catch (error) {
    console.error("Error de monitoreo:", error);
  }
}
function formatearHora(horaInput) {
  if (!horaInput) return "00:00:00";

  const [horas, minutos] = horaInput.split(":");

  return `${horas.padStart(2, '0')}:${minutos.padStart(2, '0')}:00`;
}

function descargarPDF() {
  if (!reportePDFUrl) {
    alert("No hay reporte disponible para descargar.");
    return;
  }

  window.open(reportePDFUrl, "_blank");
}
window.onload = () => {
  cargarPacientes();
  cargarPacienteActivo();
  actualizarMonitoreo();

  setInterval(actualizarMonitoreo, 3000);
  setInterval(cargarPacienteActivo, 5000);
};
setInterval(actualizarMonitoreo, 3000);
