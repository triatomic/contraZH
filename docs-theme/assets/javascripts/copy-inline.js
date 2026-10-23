// Click an inline code tag to copy its text; code blocks keep Material's copy button

function contraCopyFallback(text) {
  var area = document.createElement("textarea");
  area.value = text;
  area.setAttribute("readonly", "");
  area.style.position = "fixed";
  area.style.opacity = "0";
  document.body.appendChild(area);
  area.select();
  var ok = false;
  try {
    ok = document.execCommand("copy");
  } catch (e) {
    ok = false;
  }
  document.body.removeChild(area);
  return ok;
}

function contraFlash(code) {
  code.classList.add("contra-copied");
  setTimeout(function () {
    code.classList.remove("contra-copied");
  }, 900);
}

document.addEventListener("click", function (event) {
  var code = event.target.closest(".md-typeset code");
  if (!code || code.closest("pre") || code.closest("a")) {
    return;
  }
  var text = code.textContent;
  if (navigator.clipboard && window.isSecureContext) {
    navigator.clipboard.writeText(text).then(function () {
      contraFlash(code);
    }, function () {
      if (contraCopyFallback(text)) {
        contraFlash(code);
      }
    });
  } else if (contraCopyFallback(text)) {
    contraFlash(code);
  }
});
