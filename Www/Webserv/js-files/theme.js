const themeButton = document.getElementById("theme-toggle");
const savedTheme = localStorage.getItem("theme");

function setTheme(theme) {
  if (theme === "dark") {
    document.documentElement.setAttribute("data-theme", "dark");
    themeButton.textContent = "Light mode";
  } else {
    document.documentElement.removeAttribute("data-theme");
    themeButton.textContent = "Dark mode";
  }
}

setTheme(savedTheme === "dark" ? "dark" : "light");

themeButton.addEventListener("click", function () {
  const isDark =
    document.documentElement.getAttribute("data-theme") === "dark";

  const newTheme = isDark ? "light" : "dark";

  localStorage.setItem("theme", newTheme);
  setTheme(newTheme);
});