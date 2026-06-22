let reportePDFUrl = "";

const API_URL =
  "https://script.google.com/macros/s/AKfycbx6u1m8-Y9JKfmrSL6ATDV6dIlMogACgHCzSS8_ZSCVNA9dBJ-c9Zt3vWvovu86LtAw/exec";

let pacienteActivoID = "001";

// Buffer visual para la gráfica ECG local en SVG
let ecgBuffer = [];
const ECG_BUFFER_SIZE = 120;

// ===============================
// UTILIDADES GENERALES
// ===============================

function $(id) {
  return document.getElementById(id);
}

function setText(id, value) {
  const el = $(id);
  if (el) {
    el.textContent = value;
  }
}

function setVisible(id, visible) {
  const el = $(id);
  if (el) {
    el.style.display = visible ? "inline-block" : "none";
  }
}

function cerrarMenu() {
  const menu = $("menuToggle");
  if (menu) {
    menu.checked = false;
  }
}

function actualizarFechaHora() {
  const ahora = new Date();

  const fecha = ahora.toLocaleDateString("es-MX", {
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
  });

  const hora = ahora.toLocaleTimeString("es-MX", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });

  setText("fechaActual", fecha);
  setText("horaActual", hora);
}

function formatearFecha(fechaInput) {
  const fecha = new Date(fechaInput + "T00:00:00");

  return (
    fecha.getFullYear() +
    "-" +
    String(fecha.getMonth() + 1).padStart(2, "0") +
    "-" +
    String(fecha.getDate()).padStart(2, "0")
  );
}

function formatearHora(horaInput) {
  if (!horaInput) {
    return "00:00:00";
  }

  const [horas, minutos] = horaInput.split(":");

  return `${horas.padStart(2, "0")}:${minutos.padStart(2, "0")}:00`;
}

function valorValido(valor) {
  return (
    valor !== undefined && valor !== null && valor !== "" && Number(valor) > 0
  );
}

// ===============================
// MENÚ LATERAL
// ===============================

document.addEventListener("keydown", function (event) {
  if (event.key === "Escape") {
    cerrarMenu();
  }
});

// ===============================
// REGISTRO DE PACIENTES
// ===============================

function obtenerFactoresRiesgo() {
  const factoresSeleccionados = document.querySelectorAll(
    'input[name="factores"]:checked',
  );

  return Array.from(factoresSeleccionados)
    .map((input) => input.value)
    .join(", ");
}

async function registrarPaciente(event) {
  if (event) {
    event.preventDefault();
  }

  // ===============================
  // DATOS GENERALES
  // ===============================

  const nombre = document.getElementById("nombre")?.value.trim() || "";
  const edad = document.getElementById("edad")?.value.trim() || "";
  const sexo = document.getElementById("sexo")?.value || "";
  const peso = document.getElementById("peso")?.value.trim() || "";
  const estatura = document.getElementById("estatura")?.value.trim() || "";
  const obs = document.getElementById("obs")?.value.trim() || "";

  // ===============================
  // FACTORES DE RIESGO / PATOLOGÍAS
  // ===============================

  const factoresSeleccionados = document.querySelectorAll(
    'input[name="factores"]:checked'
  );

  const factores = Array.from(factoresSeleccionados)
    .map(input => input.value)
    .join(", ");

  // ===============================
  // UBICACIÓN
  // ===============================

  const ciudadBusqueda = document.getElementById("ciudadBusqueda")?.value.trim() || "";
  const ciudadSelect = document.getElementById("ciudad");
  const indiceCiudad = ciudadSelect?.value || "";

  let ciudad = "";
  let pais = document.getElementById("pais")?.value || "";
  let estado = document.getElementById("estado")?.value || "";
  let altitud = document.getElementById("altitud")?.value.trim() || "";
  let latitud = document.getElementById("latitud")?.value.trim() || "";
  let longitud = document.getElementById("longitud")?.value.trim() || "";

  if (
    indiceCiudad !== "" &&
    typeof ciudadesEncontradas !== "undefined" &&
    ciudadesEncontradas[Number(indiceCiudad)]
  ) {
    const lugar = ciudadesEncontradas[Number(indiceCiudad)];

    ciudad = lugar.name || ciudadBusqueda;
    pais = lugar.country || lugar.country_code || pais;
    estado = lugar.admin1 || estado;
    latitud = lugar.latitude || latitud;
    longitud = lugar.longitude || longitud;
  } else {
    ciudad = ciudadBusqueda;
  }

  // ===============================
  // VALIDACIÓN BÁSICA
  // ===============================

  if (!nombre || !edad || !sexo) {
    const resultado = document.getElementById("registroResultado");

    if (resultado) {
      resultado.innerText = "Completa al menos nombre, edad y sexo.";
    }

    return;
  }

  // ===============================
  // CONSTRUCCIÓN DE URL
  // ===============================

  const url =
    `${API_URL}?registrarPaciente=1` +
    `&nombre=${encodeURIComponent(nombre)}` +
    `&edad=${encodeURIComponent(edad)}` +
    `&sexo=${encodeURIComponent(sexo)}` +
    `&peso=${encodeURIComponent(peso)}` +
    `&estatura=${encodeURIComponent(estatura)}` +
    `&factores=${encodeURIComponent(factores)}` +
    `&pais=${encodeURIComponent(pais)}` +
    `&estado=${encodeURIComponent(estado)}` +
    `&ciudad=${encodeURIComponent(ciudad)}` +
    `&altitud=${encodeURIComponent(altitud)}` +
    `&latitud=${encodeURIComponent(latitud)}` +
    `&longitud=${encodeURIComponent(longitud)}` +
    `&obs=${encodeURIComponent(obs)}`;

  const resultado = document.getElementById("registroResultado");

  if (resultado) {
    resultado.innerText = "Registrando paciente...";
  }

  // ===============================
  // ENVÍO A APPS SCRIPT
  // ===============================

  try {
    const res = await fetch(url);

    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    const texto = await res.text();

    if (resultado) {
      resultado.innerText = texto;
    }

    const form = document.getElementById("formRegistroPaciente");

    if (form) {
      form.reset();
    }

    const ciudadResultado = document.getElementById("ciudad");
    if (ciudadResultado) {
      ciudadResultado.innerHTML = '<option value="">Busca una ciudad primero</option>';
      ciudadResultado.disabled = true;
    }

    const altitudInput = document.getElementById("altitud");
    if (altitudInput) {
      altitudInput.value = "";
    }

    if (typeof ciudadesEncontradas !== "undefined") {
      ciudadesEncontradas = [];
    }

    if (typeof cargarPacientes === "function") {
      await cargarPacientes();
    }

  } catch (error) {
    console.error("Error registrando paciente:", error);

    if (resultado) {
      resultado.innerText = "Error al registrar paciente.";
    }
  }
}

// ===============================
// CIUDAD Y ALTITUD
// ===============================
let ciudadesEncontradas = [];

async function buscarCiudades() {
  const ciudadTexto = document.getElementById("ciudadBusqueda").value.trim();

  const selectCiudad = document.getElementById("ciudad");
  const altitudInput = document.getElementById("altitud");
  const paisInput = document.getElementById("pais");
  const estadoInput = document.getElementById("estado");
  const latitudInput = document.getElementById("latitud");
  const longitudInput = document.getElementById("longitud");

  selectCiudad.innerHTML = '<option value="">Buscando...</option>';
  selectCiudad.disabled = true;

  altitudInput.value = "";
  paisInput.value = "";
  estadoInput.value = "";
  latitudInput.value = "";
  longitudInput.value = "";

  ciudadesEncontradas = [];

  if (ciudadTexto.length < 3) {
    selectCiudad.innerHTML =
      '<option value="">Escribe al menos 3 letras</option>';
    return;
  }

  const url =
    "https://geocoding-api.open-meteo.com/v1/search" +
    `?name=${encodeURIComponent(ciudadTexto)}` +
    "&count=20" +
    "&language=es" +
    "&format=json";

  try {
    const res = await fetch(url);

    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    const data = await res.json();

    ciudadesEncontradas = data.results || [];

    if (ciudadesEncontradas.length === 0) {
      selectCiudad.innerHTML =
        '<option value="">No se encontraron coincidencias</option>';
      return;
    }

    selectCiudad.innerHTML = '<option value="">Seleccionar ubicación</option>';

    ciudadesEncontradas.forEach((lugar, index) => {
      const option = document.createElement("option");

      const nombre = lugar.name || "";
      const estado = lugar.admin1 || "";
      const municipio = lugar.admin2 || "";
      const pais = lugar.country || lugar.country_code || "";
      const poblacion = lugar.population
        ? ` · Pob. ${lugar.population.toLocaleString("es-MX")}`
        : "";

      option.value = index;

      option.textContent =
        `${nombre}` +
        `${estado ? " — " + estado : ""}` +
        `${municipio ? ", " + municipio : ""}` +
        `${pais ? " · " + pais : ""}` +
        `${poblacion}`;

      selectCiudad.appendChild(option);
    });

    selectCiudad.disabled = false;

  } catch (error) {
    console.error("Error buscando ciudad:", error);

    selectCiudad.innerHTML =
      '<option value="">Error al buscar ciudad</option>';
  }
}

async function seleccionarCiudad() {
  const selectCiudad = document.getElementById("ciudad");
  const indice = selectCiudad.value;

  const altitudInput = document.getElementById("altitud");
  const paisInput = document.getElementById("pais");
  const estadoInput = document.getElementById("estado");
  const latitudInput = document.getElementById("latitud");
  const longitudInput = document.getElementById("longitud");

  if (indice === "") {
    altitudInput.value = "";
    paisInput.value = "";
    estadoInput.value = "";
    latitudInput.value = "";
    longitudInput.value = "";
    return;
  }

  const lugar = ciudadesEncontradas[Number(indice)];

  if (!lugar) {
    return;
  }

  paisInput.value = lugar.country || lugar.country_code || "";
  estadoInput.value = lugar.admin1 || "";
  latitudInput.value = lugar.latitude || "";
  longitudInput.value = lugar.longitude || "";

  if (lugar.elevation !== undefined && lugar.elevation !== null) {
    altitudInput.value = `${Math.round(lugar.elevation)} msnm`;
  } else {
    await obtenerAltitud(lugar.latitude, lugar.longitude);
  }
}

async function obtenerAltitud(latitud, longitud) {
  const altitudInput = document.getElementById("altitud");

  if (!latitud || !longitud) {
    altitudInput.value = "No disponible";
    return;
  }

  altitudInput.value = "Calculando...";

  const url =
    "https://api.open-meteo.com/v1/elevation" +
    `?latitude=${encodeURIComponent(latitud)}` +
    `&longitude=${encodeURIComponent(longitud)}`;

  try {
    const res = await fetch(url);

    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    const data = await res.json();

    if (data.elevation && data.elevation.length > 0) {
      altitudInput.value = `${Math.round(data.elevation[0])} msnm`;
    } else {
      altitudInput.value = "No disponible";
    }

  } catch (error) {
    console.error("Error obteniendo altitud:", error);
    altitudInput.value = "No disponible";
  }
}

// ===============================
// LISTA DE PACIENTES PARA REPORTE
// ===============================

async function cargarPacientes() {
  try {
    const res = await fetch(`${API_URL}?listarPacientes=1&t=${Date.now()}`);

    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    const pacientes = await res.json();

    const select = $("pacienteReporte");

    if (!select) {
      return;
    }

    select.innerHTML = '<option value="">Seleccionar paciente</option>';

    pacientes.forEach((paciente) => {
      const option = document.createElement("option");
      option.value = paciente.id;
      option.textContent = `${paciente.id} - ${paciente.nombre}`;
      select.appendChild(option);
    });
  } catch (error) {
    console.error("Error cargando pacientes:", error);
    setText("mensajeReporte", "Error cargando lista de pacientes.");
  }
}

// ===============================
// PACIENTE ACTIVO
// ===============================

async function cargarPacienteActivo() {
  try {
    const res = await fetch(`${API_URL}?pacienteActivo=1&t=${Date.now()}`);

    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    const paciente = await res.json();

    if (!paciente || !paciente.id) {
      throw new Error("No se recibió un paciente activo válido");
    }

    pacienteActivoID = paciente.id;

    setText("pacienteActivo", paciente.nombre || "Paciente sin nombre");
    setText("pacienteID", paciente.id || "---");
  } catch (error) {
    console.error("Error cargando paciente activo:", error);

    setText("pacienteActivo", "Sin paciente activo");
    setText("pacienteID", "---");
  }
}

// ===============================
// MONITOREO REMOTO
// ===============================

async function actualizarMonitoreo() {
  try {
    const url =
      `${API_URL}?accion=ultimo` +
      `&id=${encodeURIComponent(pacienteActivoID)}` +
      `&t=${Date.now()}`;

    const res = await fetch(url);

    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    const data = await res.json();

    console.log("Datos de monitoreo:", data);

    if (!data.ok) {
      setText("bpm", "--");
      setText("spo2", "--");
      setText("temp", "--");

      setText("estadoBPM", "Sin datos");
      setText("estadoSpO2", "Sin datos");
      setText("estadoTemp", "Sin datos");
      setText("estadoECG", "Sin datos");

      return;
    }

    if (valorValido(data.bpm)) {
      setText("bpm", data.bpm);
      setText("estadoBPM", "Dato recibido");
    } else {
      setText("bpm", "--");
      setText("estadoBPM", "Sin dato disponible");
    }

    if (valorValido(data.spo2)) {
      setText("spo2", data.spo2);
      setText("estadoSpO2", "Dato recibido");
    } else {
      setText("spo2", "--");
      setText("estadoSpO2", "Sin dato disponible");
    }

    if (valorValido(data.temp)) {
      setText("temp", Number(data.temp).toFixed(1));
      setText("estadoTemp", "Dato recibido");
    } else {
      setText("temp", "--");
      setText("estadoTemp", "Sin dato disponible");
    }

    if (data.ecg !== undefined && data.ecg !== null && data.ecg !== "") {
      setText("estadoECG", "Señal recibida");
      actualizarGraficaECG(Number(data.ecg));
    } else {
      setText("estadoECG", "ECG no disponible en nube");
      actualizarGraficaECG(null);
    }

    setText("estadoConexion", "En línea");
  } catch (error) {
    console.error("Error de monitoreo:", error);

    setText("bpm", "--");
    setText("spo2", "--");
    setText("temp", "--");

    setText("estadoBPM", "Error de lectura");
    setText("estadoSpO2", "Error de lectura");
    setText("estadoTemp", "Error de lectura");
    setText("estadoConexion", "Sin conexión");
  }
}

// ===============================
// GRÁFICA ECG VISUAL
// ===============================

function actualizarGraficaECG(valorADC) {
  const polyline = document.querySelector("#graficaECG svg polyline");

  if (!polyline) {
    return;
  }

  let y;

  if (valorADC === null || isNaN(valorADC)) {
    y = 140;
  } else {
    // ADC 12 bits: 0 a 4095.
    // Se comprime visualmente para quedar dentro de la gráfica.
    y = 220 - (valorADC / 4095) * 180;

    if (y < 30) y = 30;
    if (y > 230) y = 230;
  }

  ecgBuffer.push(y);

  if (ecgBuffer.length > ECG_BUFFER_SIZE) {
    ecgBuffer.shift();
  }

  const ancho = 1000;
  const paso = ancho / (ECG_BUFFER_SIZE - 1);

  const puntos = ecgBuffer
    .map((valorY, index) => {
      const x = Math.round(index * paso);
      return `${x},${Math.round(valorY)}`;
    })
    .join(" ");

  polyline.setAttribute("points", puntos);
}

// ===============================
// REPORTES
// ===============================

async function generarReporte(event) {
  if (event) {
    event.preventDefault();
  }

  const id = $("pacienteReporte")?.value.trim() || "";
  const iniRaw = $("fechaInicio")?.value || "";
  const finRaw = $("fechaFin")?.value || "";
  const horaIniRaw = $("horaInicio")?.value || "";
  const horaFinRaw = $("horaFin")?.value || "";

  const linkReporte = $("linkReporte");

  if (linkReporte) {
    linkReporte.style.display = "none";
    linkReporte.removeAttribute("href");
  }

  reportePDFUrl = "";

  if (!id || !iniRaw || !finRaw || !horaIniRaw || !horaFinRaw) {
    setText("estadoReporte", "Datos incompletos");
    setText(
      "mensajeReporte",
      "Completa todos los campos para generar el reporte.",
    );
    return;
  }

  const ini = formatearFecha(iniRaw);
  const fin = formatearFecha(finRaw);
  const horaIni = formatearHora(horaIniRaw);
  const horaFin = formatearHora(horaFinRaw);

  const url =
    `${API_URL}?reporte=1` +
    `&id=${encodeURIComponent(id)}` +
    `&ini=${encodeURIComponent(ini)}` +
    `&fin=${encodeURIComponent(fin)}` +
    `&horaIni=${encodeURIComponent(horaIni)}` +
    `&horaFin=${encodeURIComponent(horaFin)}`;

  setText("estadoReporte", "Generando reporte...");
  setText(
    "mensajeReporte",
    "Espera mientras se filtran los datos y se genera el PDF.",
  );

  try {
    const res = await fetch(url);

    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    const data = await res.json();

    setText(
      "estadoReporte",
      data.ok ? "Reporte generado" : "No se pudo generar",
    );
    setText("mensajeReporte", data.mensaje || "Proceso finalizado.");

    if (data.ok && data.pdfUrl) {
      reportePDFUrl = data.pdfUrl;

      if (linkReporte) {
        linkReporte.href = reportePDFUrl;
        linkReporte.target = "_blank";
        linkReporte.style.display = "inline-block";
      }
    }
  } catch (error) {
    console.error("Error generando reporte:", error);

    setText("estadoReporte", "Error");
    setText("mensajeReporte", `Error al generar reporte: ${error.message}`);
  }
}

function descargarPDF() {
  if (!reportePDFUrl) {
    alert("No hay reporte disponible para descargar.");
    return;
  }

  window.open(reportePDFUrl, "_blank");
}

// ===============================
// INICIALIZACIÓN
// ===============================

function inicializarEventos() {
  const formRegistro = $("formRegistroPaciente");
  if (formRegistro) {
    formRegistro.addEventListener("submit", registrarPaciente);
  }

  const formReporte = $("formReporte");
  if (formReporte) {
    formReporte.addEventListener("submit", generarReporte);
  }

  const linkReporte = $("linkReporte");
  if (linkReporte) {
    linkReporte.style.display = "none";
  }

  // Inicializa la línea ECG centrada
  ecgBuffer = Array(ECG_BUFFER_SIZE).fill(140);
  actualizarGraficaECG(null);
}

window.addEventListener("DOMContentLoaded", async () => {
  inicializarEventos();
  actualizarFechaHora();

  await cargarPacientes();
  await cargarPacienteActivo();
  await actualizarMonitoreo();

  setInterval(actualizarFechaHora, 1000);
  setInterval(actualizarMonitoreo, 3000);
  setInterval(cargarPacienteActivo, 5000);
});
