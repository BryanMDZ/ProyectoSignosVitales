const API_URL = "https://script.google.com/macros/s/AKfycbwayo2dCadZQoLEDbvPAuDtKTDrsldJDPWSBtH9w-VHaHTtcEpU-H4xgmuT_oKUYSOJ/exec";

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

async function generarReporte() {
  const id = document.getElementById("pacienteID").value;
  function formatearFecha(fechaInput) {
  const fecha = new Date(fechaInput);

  return fecha.getFullYear() + "-" +
    String(fecha.getMonth() + 1).padStart(2, '0') + "-" +
    String(fecha.getDate()).padStart(2, '0');
  }
  const horaIni = formatearHora(
  document.getElementById("horaInicio").value
  );
  
  const horaFin = formatearHora(
    document.getElementById("horaFin").value
  );

  const url = `${API_URL}?reporte=1&id=${id}&ini=${ini}&fin=${fin}&horaIni=${horaIni}&horaFin=${horaFin}`;

  const res = await fetch(url);
  const texto = await res.text();

  document.getElementById("reporteResultado").innerText = texto;
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
setInterval(actualizarMonitoreo, 3000);
