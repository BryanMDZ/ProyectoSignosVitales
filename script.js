const API_URL = "https://script.google.com/macros/s/AKfycbwaitYou653R8BhCAgNV9Bej5jnxBQYyxxGokC3nodF8dtO4GZXYV8t4D6Ve6Pw0gPS/exec";

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
  }
}

async function generarReporte() {
  const id = document.getElementById("pacienteID").value.trim();
  const iniRaw = document.getElementById("fechaInicio").value;
  const finRaw = document.getElementById("fechaFin").value;
  const horaIniRaw = document.getElementById("horaInicio").value;
  const horaFinRaw = document.getElementById("horaFin").value;

  const resultado = document.getElementById("reporteResultado");

  if (!id || !iniRaw || !finRaw || !horaIniRaw || !horaFinRaw) {
    resultado.innerText = "Completa todos los campos para generar el reporte.";
    return;
  }

  function formatearFecha(fechaInput) {
    const fecha = new Date(fechaInput + "T00:00:00");

    return fecha.getFullYear() + "-" +
      String(fecha.getMonth() + 1).padStart(2, '0') + "-" +
      String(fecha.getDate()).padStart(2, '0');
  }

  function formatearHora(horaInput) {
    const [horas, minutos] = horaInput.split(":");

    return `${horas.padStart(2, '0')}:${minutos.padStart(2, '0')}:00`;
  }

  const ini = formatearFecha(iniRaw);
  const fin = formatearFecha(finRaw);
  const horaIni = formatearHora(horaIniRaw);
  const horaFin = formatearHora(horaFinRaw);

  const url = `${API_URL}?reporte=1&id=${encodeURIComponent(id)}&ini=${ini}&fin=${fin}&horaIni=${horaIni}&horaFin=${horaFin}`;

  resultado.innerText = "Generando reporte...";

  try {
    const res = await fetch(url);

    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    const texto = await res.text();

    resultado.innerText = texto || "Reporte generado correctamente.";

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
window.onload = () => {
  cargarPacientes();
  actualizarMonitoreo();
  setInterval(actualizarMonitoreo, 3000);
};
setInterval(actualizarMonitoreo, 3000);
