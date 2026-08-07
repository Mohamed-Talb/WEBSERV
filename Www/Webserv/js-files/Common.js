function wsUrl(path) {
  return new URL(path, window.location.origin).toString();
}

function wsLog(element, message, className) {
  const line = document.createElement("div");
  line.textContent = message;

  if (className)
    line.classList.add(className);

  element.appendChild(line);
}

function wsAddEmptyLines(element, count) {
  for (let i = 0; i < count; i++)
    element.appendChild(document.createElement("br"));
}

function wsEscape(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}