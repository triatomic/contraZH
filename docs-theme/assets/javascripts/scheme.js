// Colour scheme switch; runs in <head> so the saved scheme applies before the page draws

(function () {
  var KEY = "contra-scheme";
  var SCHEMES = [["contra", "Contra"], ["slate", "Slate"]];

  function load() {
    try {
      return localStorage.getItem(KEY) === "slate" ? "slate" : "contra";
    } catch (e) {
      return "contra";
    }
  }

  function save(scheme) {
    try {
      localStorage.setItem(KEY, scheme);
    } catch (e) {
      return;
    }
  }

  function apply(scheme) {
    if (scheme === "slate") {
      document.documentElement.setAttribute("data-scheme", "slate");
    } else {
      document.documentElement.removeAttribute("data-scheme");
    }
  }

  apply(load());

  document.addEventListener("DOMContentLoaded", function () {
    var header = document.querySelector(".wy-side-nav-search");
    if (!header) {
      return;
    }
    var group = document.createElement("div");
    group.className = "contra-scheme";
    group.setAttribute("role", "radiogroup");
    group.setAttribute("aria-label", "Colour scheme");
    var current = load();
    SCHEMES.forEach(function (entry) {
      var button = document.createElement("button");
      button.type = "button";
      button.textContent = entry[1];
      button.setAttribute("role", "radio");
      button.setAttribute("data-scheme", entry[0]);
      button.setAttribute("aria-checked", entry[0] === current ? "true" : "false");
      button.addEventListener("click", function () {
        apply(entry[0]);
        save(entry[0]);
        var all = group.querySelectorAll("button");
        for (var i = 0; i < all.length; i++) {
          all[i].setAttribute("aria-checked", all[i] === button ? "true" : "false");
        }
      });
      group.appendChild(button);
    });
    header.insertBefore(group, header.firstChild);
  });
})();
