// Search as you type over the Pagefind index; Enter with nothing picked still opens Sphinx's search page

(function () {
  var MAX_PAGES = 8;
  var MAX_SECTIONS = 3;
  var root = new URL(document.documentElement.getAttribute("data-content_root") || "./", location.href);
  var pagefind = null;

  function loadPagefind() {
    if (!pagefind) {
      pagefind = import(root.href + "pagefind/pagefind.js").then(function (module) {
        return module.options({ baseUrl: root.pathname }).then(function () {
          return module;
        });
      });
    }
    return pagefind;
  }

  function entry(href, title, excerpt, section) {
    var link = document.createElement("a");
    link.href = href;
    link.className = section ? "contra-search__item contra-search__item--section" : "contra-search__item";
    link.setAttribute("role", "option");
    var heading = document.createElement("span");
    heading.className = "contra-search__title";
    heading.textContent = title;
    var text = document.createElement("span");
    text.className = "contra-search__excerpt";

    // Pagefind escapes page text and adds only <mark> tags
    text.innerHTML = excerpt;

    link.appendChild(heading);
    link.appendChild(text);
    return link;
  }

  function sections(page) {
    var subs = (page.sub_results || []).filter(function (sub) {
      return sub.url !== page.url;
    });
    subs.sort(function (a, b) {
      return (b.locations || []).length - (a.locations || []).length;
    });
    return subs.slice(0, MAX_SECTIONS);
  }

  document.addEventListener("DOMContentLoaded", function () {
    var input = document.querySelector(".wy-side-nav-search input[name='q']");
    if (!input) {
      return;
    }
    var panel = document.createElement("div");
    panel.className = "contra-search";
    panel.id = "contra-search";
    panel.setAttribute("role", "listbox");
    panel.hidden = true;
    document.body.appendChild(panel);
    input.setAttribute("autocomplete", "off");
    input.setAttribute("aria-controls", panel.id);
    input.setAttribute("aria-expanded", "false");
    var active = -1;

    function place() {
      var box = input.getBoundingClientRect();
      panel.style.top = (box.bottom + 6) + "px";
      panel.style.left = Math.max(8, Math.min(box.left, window.innerWidth - panel.offsetWidth - 8)) + "px";
    }

    function hide() {
      panel.hidden = true;
      active = -1;
      input.setAttribute("aria-expanded", "false");
    }

    function show(nodes) {
      panel.replaceChildren.apply(panel, nodes);
      panel.hidden = false;
      active = -1;
      input.setAttribute("aria-expanded", "true");
      place();
    }

    function highlight(index) {
      var links = panel.querySelectorAll("a");
      if (!links.length) {
        return;
      }
      active = (index + links.length) % links.length;
      for (var i = 0; i < links.length; i++) {
        links[i].classList.toggle("is-active", i === active);
        links[i].setAttribute("aria-selected", i === active ? "true" : "false");
      }
      links[active].scrollIntoView({ block: "nearest" });
    }

    function run() {
      var term = input.value.trim();
      if (!term) {
        hide();
        return;
      }
      loadPagefind().then(function (module) {
        return module.debouncedSearch(term, {}, 150);
      }).then(function (search) {
        if (search === null || input.value.trim() !== term) {
          return null;
        }
        return Promise.all(search.results.slice(0, MAX_PAGES).map(function (result) {
          return result.data();
        }));
      }).then(function (pages) {
        if (pages === null) {
          return;
        }
        if (!pages.length) {
          var empty = document.createElement("div");
          empty.className = "contra-search__empty";
          empty.textContent = "No results for “" + term + "”";
          show([empty]);
          return;
        }
        var nodes = [];
        pages.forEach(function (page) {
          nodes.push(entry(page.url, page.meta.title || page.url, page.excerpt, false));
          sections(page).forEach(function (sub) {
            nodes.push(entry(sub.url, sub.title, sub.excerpt, true));
          });
        });
        show(nodes);
      }).catch(function () {
        hide();
      });
    }

    input.addEventListener("focus", loadPagefind);
    input.addEventListener("input", run);
    input.addEventListener("blur", hide);
    input.addEventListener("keydown", function (event) {
      if (panel.hidden) {
        return;
      }
      if (event.key === "ArrowDown" || event.key === "ArrowUp") {
        event.preventDefault();
        highlight(active + (event.key === "ArrowDown" ? 1 : -1));
      } else if (event.key === "Enter" && active >= 0) {
        event.preventDefault();
        location.href = panel.querySelectorAll("a")[active].href;
      } else if (event.key === "Escape") {
        hide();
      }
    });

    // Keep focus in the input so a click on a result is not lost to blur
    panel.addEventListener("mousedown", function (event) {
      event.preventDefault();
    });

    window.addEventListener("resize", function () {
      if (!panel.hidden) {
        place();
      }
    });
  });
})();
