#ifndef HTML_TEMPLATES_H
#define HTML_TEMPLATES_H

#include "../lib/string.h"

// --- THE FULL HOMEPAGE ---
static const char HOMEPAGE_HTML[] = R"raw(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Seamus the Search Engine</title>
<link rel="icon" type="image/png" href="/htmlserver/images/tank-favicon.png">
<style>
  body {
    margin: 0; padding: 0; min-height: 100vh;
    display: flex; flex-direction: column; align-items: center;
    background: linear-gradient(to top, #5375c9 0%, #ffffff 85%, #ffffff 100%);
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  }
  .top-right-logo { position: absolute; top: 25px; right: 30px; width: 80px; height: auto; transition: opacity 0.2s; }
  .top-right-logo:hover { opacity: 0.8; }
  .header { margin-top: 15vh; text-align: center; }
  .main-seamus-logo { width: 750px; max-width: 90vw; height: auto; margin-bottom: 50px; }
  .search-container { display: flex; align-items: center; justify-content: center; width: 100%; }
  .search-box {
    display: flex; align-items: center; width: 60%; max-width: 800px;
    box-shadow: 0 8px 24px rgba(0,0,0,0.15); border-radius: 40px;
    background: white; overflow: hidden; border: 1px solid #e0e0e0;
  }
  #q { flex: 1; border: none; padding: 20px 30px; font-size: 1.5rem; outline: none; background: transparent; }
  #go { border: none; background: transparent; padding: 15px 25px; cursor: pointer; display: flex; align-items: center; justify-content: center; }
  #go img { width: 30px; height: 30px; opacity: 0.7; transition: opacity 0.2s; }
  #go:hover img { opacity: 1; }
  .footer { margin-top: auto; width: 100%; display: flex; justify-content: space-evenly; padding: 30px 10px; font-size: 0.9rem; color: rgba(255, 255, 255, 0.9); box-sizing: border-box; }
  .footer a { white-space: nowrap; text-decoration: none; color: inherit; transition: opacity 0.2s; }
  .footer a:hover { opacity: 0.7; }
</style>
</head>
<body>
  <a href="https://tradersatmichigan.com" target="_blank">
    <img src="/htmlserver/images/tam-logo.png" class="top-right-logo" alt="TAM Logo">
  </a>
  <div class="header">
    <img src="/htmlserver/images/seamusfinalphoto.png" class="main-seamus-logo" alt="Seamus the Search Engine">
  </div>
  <div class="search-container">
    <div class="search-box">
      <input id="q" type="text" autofocus>
      <button id="go"><img src="/htmlserver/images/magnifying-glass.png" alt="Search"></button>
    </div>
  </div>
  <div class="footer">
    <a href="https://www.linkedin.com/in/erikvnielsen/" target="_blank">Erik Nielsen</a>
    <a href="https://www.linkedin.com/in/amizhen/" target="_blank">Aiden Mizhen</a>
    <a href="https://www.linkedin.com/in/dmcde/" target="_blank">David McDermott</a>
    <a href="https://www.linkedin.com/in/charles-huang-02a123205/" target="_blank">Charles Huang</a>
    <a href="https://www.linkedin.com/in/hbagalkote/" target="_blank">Hrishkesh Bagalkote</a>
    <a href="https://www.linkedin.com/in/esben-sorensen/" target="_blank">Esben Sorensen</a>
  </div>
  <script>
    function submit(){
      var v=document.getElementById('q').value.trim();
      if(!v)return;
      window.location.href='/'+encodeURIComponent(v);
    }
    document.getElementById('go').addEventListener('click',submit);
    document.getElementById('q').addEventListener('keydown',function(e){ if(e.key==='Enter') submit(); });
  </script>
</body>
</html>
)raw";

// --- THE FULL RESULTS PAGE (PART 1: CSS & TOP BAR) ---
static const char RESULTS_PART_1[] = R"raw(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Search Results</title>
<link rel="icon" type="image/png" href="/htmlserver/images/tank-favicon.png">
<style>
  body { margin: 0; padding: 30px 40px; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #ffffff; box-sizing: border-box; }
  .top-bar { display: flex; align-items: center; gap: 15px; margin-bottom: 30px; border-bottom: 1px solid #eee; padding-bottom: 20px; width: 100%; }
  .top-right-logo { position: absolute; top: 25px; right: 40px; width: 70px; height: auto; transition: opacity 0.2s; }
  .header-title { font-size: 1.8rem; color: #1a1a1a; margin: 0; margin-left: auto; margin-right: 90px; white-space: nowrap; }
  .search-box { padding: 10px 20px; font-size: 1.2rem; border-radius: 30px; border: 1px solid #dfe1e5; width: 400px; outline: none; background: #f8f9fa; color: #333; }
  .back-btn { padding: 10px 20px; font-size: 1rem; color: white; background-color: #2e5c31; border: none; border-radius: 30px; text-decoration: none; cursor: pointer; transition: background-color 0.2s; font-weight: bold; }
  .back-btn:hover { background-color: #1e3c20; }
  .results-container { display: flex; flex-direction: column; gap: 24px; width: 90%; max-width: 1200px; }
  .result-item { display: flex; flex-direction: column; text-align: left; }
  .result-title { font-size: 1.4rem; color: #1a0dab; text-decoration: none; margin-bottom: 3px; }
  .result-title:hover { text-decoration: underline; }
  .result-url { font-size: 0.95rem; color: #006621; text-decoration: none; }
  .pagination { display: flex; justify-content: left; gap: 15px; margin-top: 30px; padding-bottom: 40px; }
  .page-btn { padding: 8px 16px; font-size: 1rem; color: #1a0dab; background-color: #fff; border: 1px solid #dfe1e5; border-radius: 4px; text-decoration: none; }
  .bottom-right-container { position: fixed; bottom: 0; right: 0; display: flex; flex-direction: column; align-items: flex-end; z-index: 10; pointer-events: none; }
  .response-time-box { background-color: rgba(255, 255, 255, 0.95); border: 1px solid #dfe1e5; border-radius: 8px; padding: 10px 15px; box-shadow: 0 4px 10px rgba(0,0,0,0.15); margin-right: 30px; margin-bottom: 10px; font-size: 0.9rem; color: #333; text-align: center; }
  .response-time-title { font-weight: bold; margin-bottom: 4px; color: #1a0dab; font-size: 0.8rem; text-transform: uppercase; }
  .bottom-right-hatt { height: 35vh; max-height: 400px; width: auto; }
</style>
</head>
<body>
  <a href="https://tradersatmichigan.com" target="_blank"><img src="/htmlserver/images/tam-logo.png" class="top-right-logo"></a>
  <div class="top-bar">
)raw";

// --- THE FULL RESULTS PAGE (PART 2: FOOTER, TIMER & HATT) ---
static const char RESULTS_PART_2[] = R"raw(
  <div class="bottom-right-container">
    <div class="response-time-box">
      <div class="response-time-title">Query Response Time</div>
)raw";

static const char RESULTS_PART_3[] = R"raw(
      ms
    </div>
    <img src="/htmlserver/images/sirtophamhatt.png" class="bottom-right-hatt" alt="Sir Topham Hatt">
  </div>
</body>
</html>
)raw";

#endif