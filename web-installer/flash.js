// Mostra a versão do firmware embutido (gerada pela CI em firmware/manifest.json).
fetch('firmware/manifest.json')
    .then((r) => (r.ok ? r.json() : null))
    .then((m) => {
        if (!m) return;
        const tag = document.getElementById('build-tag');
        if (tag && m.version) tag.textContent = `KIT Core · ${m.version}`;
    })
    .catch(() => {});
