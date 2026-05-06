const API_URL = "https://script.google.com/macros/s/AKfycbzQel0BLDz4dgQS-xENEKc0FPHjmv3Sy7WFwx3N9prynagKcqKIQZ7k52XG3FmhAVRt/exec";

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
  const ini = document.getElementById("fechaInicio").value;
  const fin = document.getElementById("fechaFin").value;
  const horaIni = document.getElementById("horaInicio").value + ":00";
  const horaFin = document.getElementById("horaFin").value + ":59";

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

setInterval(actualizarMonitoreo, 3000);
