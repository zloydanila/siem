(function(){
  const form = document.getElementById("loginForm");
  const u = document.getElementById("username");
  const p = document.getElementById("password");
  const err = document.getElementById("err");

  if (!form) return;

  function makeBasicAuth(username, password) {
    const token = username + ":" + password;
    return btoa(unescape(encodeURIComponent(token)));
  }

  async function ping(username, password) {
    const auth = makeBasicAuth(username, password);
    

    const response = await fetch("/api/dashboard", {
      method: "GET",
      headers: { 
        "Authorization": `Basic ${auth}`,
        "Accept": "application/json"
      },
      credentials: "omit",
      cache: "no-store"
    });


    return response.status === 200;
  }

  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    err.textContent = "";

    const username = u.value.trim();
    const password = p.value;

    if (!username || !password) {
      err.textContent = "Логин и пароль обязательны";
      (username ? p : u).focus();
      return;
    }

    try {
      const success = await ping(username, password);
      if (!success) {
        err.textContent = "Неверный логин или пароль";
        u.value = "";
        p.value = "";
        u.focus();
        return;
      }

      const auth = btoa(username + ":" + password);
      sessionStorage.setItem("siem_auth", auth);
      location.href = "/dashboard";
    } catch (ex) {
      console.error("Login error:", ex);
      err.textContent = "Сервер недоступен";
      p.value = "";
      p.focus();
    }
  });
})();
