const fields = {
	scaleMode: document.getElementById("scaleMode"),
	monitorDiagonal: document.getElementById("monitorDiagonal"),
	boardCheckboxes: document.getElementById("boardCheckboxes"),
	calibrationBarPx: document.getElementById("calibrationBarPx"),
	calibrationMeasuredMm: document.getElementById("calibrationMeasuredMm"),
	monitorSettings: document.getElementById("monitorSettings"),
	calibrationSection: document.getElementById("calibrationSection"),
	calibrationBarWrap: document.getElementById("calibrationBarWrap"),
	calibrationBar: document.getElementById("calibrationBar"),
	calibrationActual: document.getElementById("calibrationActual"),
	metrics: document.getElementById("metrics"),
	error: document.getElementById("error"),
	gallery: document.getElementById("gallery"),
	toggleSidebar: document.getElementById("toggleSidebar"),
	zoomSelect: document.getElementById("zoomSelect"),
};

let defaults = null;
let screenshotGroups = [];
let boards = [];

const SIDEBAR_COLLAPSED_KEY = "goldenViewer.sidebarCollapsed";

function parsePositive(value) {
	const n = Number(value);
	if (!Number.isFinite(n) || n <= 0) {
		return null;
	}
	return n;
}

function browserMonitorPixels() {
	const dpr = window.devicePixelRatio || 1;
	return {
		width: Math.round(window.screen.width * dpr),
		height: Math.round(window.screen.height * dpr),
	};
}

function readStoredSidebarCollapsed() {
	try {
		return window.localStorage.getItem(SIDEBAR_COLLAPSED_KEY) === "1";
	} catch {
		return false;
	}
}

function writeStoredSidebarCollapsed(collapsed) {
	try {
		window.localStorage.setItem(SIDEBAR_COLLAPSED_KEY, collapsed ? "1" : "0");
	} catch {
		// Ignore storage failures in restrictive browser modes.
	}
}

function setSidebarCollapsed(collapsed, persist = true) {
	document.body.classList.toggle("settings-collapsed", collapsed);
	fields.toggleSidebar.textContent = collapsed
		? "Show settings"
		: "Hide settings";
	fields.toggleSidebar.setAttribute("aria-expanded", String(!collapsed));
	if (persist) {
		writeStoredSidebarCollapsed(collapsed);
	}
}

function encodePath(path) {
	return path
		.split("/")
		.filter((segment) => segment.length > 0)
		.map((segment) => encodeURIComponent(segment))
		.join("/");
}

function boardById(boardId) {
	return boards.find((item) => item.id === boardId) || null;
}

function boardLabel(boardId) {
	const board = boardById(boardId);
	if (!board) {
		return boardId;
	}
	const sizeText = `${board.width}x${board.height}`;
	const diagonalText =
		board.diagonal_inches == null
			? "unknown size"
			: `${board.diagonal_inches.toFixed(1)}"`;
	return `${boardId} (${sizeText}, ${diagonalText})`;
}

function selectedBoardIds() {
	const checkboxes = fields.boardCheckboxes.querySelectorAll(
		'input[type="checkbox"]',
	);
	const selected = [];
	for (const cb of checkboxes) {
		if (cb.checked) {
			selected.push(cb.dataset.boardId);
		}
	}
	return selected;
}

function targetPpiForBoard(boardId) {
	const board = boardById(boardId);
	if (!board || board.diagonal_inches == null) {
		return null;
	}
	return Math.hypot(board.width, board.height) / board.diagonal_inches;
}

async function loadData() {
	const [configResponse, screenshotsResponse] = await Promise.all([
		fetch("/api/config"),
		fetch("/api/screenshots"),
	]);

	if (!configResponse.ok) {
		throw new Error("Failed to load viewer config");
	}
	if (!screenshotsResponse.ok) {
		throw new Error("Failed to load screenshot list");
	}

	const configJson = await configResponse.json();
	const screenshotsJson = await screenshotsResponse.json();

	defaults = configJson.defaults;
	screenshotGroups = screenshotsJson.groups;
	boards = screenshotsJson.boards;
}

function setDefaults() {
	fields.scaleMode.value = "specs";
	fields.monitorDiagonal.value = defaults.monitor_diagonal_in;
	fields.calibrationBarPx.value = "200";
	fields.calibrationMeasuredMm.value = "";
	fields.zoomSelect.value = "fit";
}

function configureBoardMode() {
	if (boards.length === 0) {
		throw new Error("No boards available");
	}

	fields.boardCheckboxes.innerHTML = "";
	for (const board of boards) {
		const label = document.createElement("label");
		label.className = "board-checkbox-label";

		const cb = document.createElement("input");
		cb.type = "checkbox";
		cb.checked = true;
		cb.dataset.boardId = board.id;
		cb.id = `boardCheck_${board.id}`;
		cb.addEventListener("change", () => {
			buildScreens();
			updateScale();
		});

		const span = document.createElement("span");
		span.textContent = boardLabel(board.id);

		label.appendChild(cb);
		label.appendChild(span);
		fields.boardCheckboxes.appendChild(label);
	}
}

function buildScreens() {
	fields.gallery.innerHTML = "";
	const activeBoardIds = selectedBoardIds();

	for (const group of screenshotGroups) {
		const card = document.createElement("article");
		card.className = "screen";

		const meta = document.createElement("div");
		meta.className = "screen-meta";

		const name = document.createElement("div");
		name.className = "screen-name";
		name.textContent = group.name;

		const availability = document.createElement("div");
		availability.className = "screen-size";
		const availableIds = activeBoardIds.filter(
			(boardId) => group.by_board[boardId],
		);
		availability.textContent = `${availableIds.length}/${activeBoardIds.length} shown`;

		const variants = document.createElement("div");
		variants.className = "screen-variants";

		const colWeights = activeBoardIds.map((boardId) => {
			const shot = group.by_board[boardId];
			const board = boardById(boardId);
			const maxDim = shot
				? Math.max(shot.width, shot.height)
				: board
					? Math.max(board.width, board.height)
					: 1;
			const targetPpi = targetPpiForBoard(boardId);
			return targetPpi !== null ? maxDim / targetPpi : maxDim;
		});
		for (const [i, boardId] of activeBoardIds.entries()) {
			const shot = group.by_board[boardId];
			const variant = document.createElement("section");
			variant.className = "screen-variant";
			variant.style.flex = `${colWeights[i]} 0 ${COL_OVERHEAD_PX}px`;

			const variantMeta = document.createElement("div");
			variantMeta.className = "variant-meta";

			const variantTitle = document.createElement("div");
			variantTitle.className = "variant-resolution";
			variantTitle.textContent = boardLabel(boardId);

			const variantDetails = document.createElement("div");
			variantDetails.className = "variant-size";

			variantMeta.appendChild(variantTitle);
			variantMeta.appendChild(variantDetails);
			variant.appendChild(variantMeta);

			if (shot) {
				variantDetails.textContent = `${shot.width}x${shot.height} (${shot.orientation})`;

				const wrap = document.createElement("div");
				wrap.className = "image-wrap";

				const img = document.createElement("img");
				img.src = `/golden/${encodePath(shot.path)}`;
				img.alt = `${group.name} ${boardId}`;
				img.width = shot.width;
				img.height = shot.height;
				img.dataset.width = String(shot.width);
				img.dataset.height = String(shot.height);
				img.dataset.board = boardId;

				wrap.appendChild(img);
				variant.appendChild(wrap);
			} else {
				variantDetails.textContent = "missing";
				const missing = document.createElement("div");
				missing.className = "variant-missing";
				missing.textContent = "No screenshot for this board";
				variant.appendChild(missing);
			}

			variants.appendChild(variant);
		}

		meta.appendChild(name);
		meta.appendChild(availability);
		card.appendChild(meta);
		card.appendChild(variants);
		fields.gallery.appendChild(card);
	}
}

function calibrationBarPixels() {
	const requestedPx = parsePositive(fields.calibrationBarPx.value);
	if (requestedPx === null) {
		fields.calibrationBar.style.width = "20px";
		fields.calibrationActual.textContent =
			"Enter a positive calibration bar length.";
		return null;
	}

	const maxVisiblePx = Math.max(
		20,
		Math.floor(fields.calibrationBarWrap.clientWidth - 20),
	);
	const actualPx = Math.max(20, Math.min(requestedPx, maxVisiblePx));

	fields.calibrationBar.style.width = `${actualPx.toFixed(0)}px`;
	if (requestedPx > maxVisiblePx) {
		fields.calibrationActual.textContent = `Requested ${requestedPx.toFixed(0)} CSS px, using ${actualPx.toFixed(0)} CSS px so the bar stays fully visible.`;
	} else {
		fields.calibrationActual.textContent = `Using ${actualPx.toFixed(0)} CSS px calibration bar.`;
	}

	return actualPx;
}

function applyModeVisibility() {
	const isSpecs = fields.scaleMode.value === "specs";
	fields.monitorSettings.classList.toggle("hidden", !isSpecs);
	fields.calibrationSection.classList.toggle("hidden", isSpecs);
}

// Fixed CSS overhead per column: variant border (2) + padding (20) +
// wrap border (2) + padding (20) = 44px.  Used as flex-basis so flexbox
// reserves the overhead equally and distributes only the remainder.
const COL_OVERHEAD_PX = 44;

function computeFitZoom(cssPpi) {
	let minFitZoom = Infinity;

	for (const img of fields.gallery.querySelectorAll(
		"img[data-width][data-height][data-board]",
	)) {
		const pxW = Number(img.dataset.width);
		const boardId = img.dataset.board;
		const targetPpi = targetPpiForBoard(boardId);
		if (!Number.isFinite(pxW) || !targetPpi) {
			continue;
		}

		const wrap = img.closest(".image-wrap");
		if (!wrap) {
			continue;
		}

		// Subtract both paddings: overflow fires when image exceeds the content
		// box (clientWidth - paddingLeft - paddingRight).
		const availableWidth = wrap.clientWidth - 20;
		if (availableWidth <= 0) {
			continue;
		}

		const scaledW = (pxW * cssPpi) / targetPpi;
		minFitZoom = Math.min(minFitZoom, availableWidth / scaledW);
	}

	return Number.isFinite(minFitZoom) && minFitZoom > 0 ? minFitZoom : 1;
}

function renderScaledImages(cssPpi, zoom) {
	const missingProfiles = new Set();

	for (const img of fields.gallery.querySelectorAll(
		"img[data-width][data-height][data-board]",
	)) {
		const pxW = Number(img.dataset.width);
		const pxH = Number(img.dataset.height);
		const boardId = img.dataset.board;

		if (!Number.isFinite(pxW) || !Number.isFinite(pxH) || !boardId) {
			continue;
		}

		const targetPpi = targetPpiForBoard(boardId);
		if (targetPpi === null) {
			missingProfiles.add(boardId);
			continue;
		}

		const cssScale = (cssPpi / targetPpi) * zoom;
		img.style.width = `${(pxW * cssScale).toFixed(2)}px`;
		img.style.height = `${(pxH * cssScale).toFixed(2)}px`;
	}

	return [...missingProfiles];
}

function updateScale() {
	fields.error.textContent = "";
	applyModeVisibility();

	let cssPpi = null;
	const metrics = [];
	const activeBoardIds = selectedBoardIds();
	metrics.push(
		`<div>Active boards: <code>${activeBoardIds.join(", ") || "none"}</code></div>`,
	);

	const boardMetrics = boards
		.map((board) => board.id)
		.map((boardId) => {
			const targetPpi = targetPpiForBoard(boardId);
			const board = boardById(boardId);
			if (targetPpi === null || !board) {
				return `Board <code>${boardId}</code>: missing device-size profile`;
			}
			return `Board <code>${boardId}</code>: ${board.diagonal_inches.toFixed(1)}&quot;, ${board.width}x${board.height}, target PPI <code>${targetPpi.toFixed(2)}</code>`;
		});
	metrics.push(...boardMetrics.map((line) => `<div>${line}</div>`));

	if (fields.scaleMode.value === "specs") {
		const monitorDiagonal = parsePositive(fields.monitorDiagonal.value);
		const dpr = window.devicePixelRatio || 1;
		const browser = browserMonitorPixels();
		const monitorWidth = browser.width;
		const monitorHeight = browser.height;

		if (monitorDiagonal === null) {
			fields.error.textContent = "Enter a valid monitor diagonal size.";
			return;
		}

		const monitorPpi =
			Math.hypot(monitorWidth, monitorHeight) / monitorDiagonal;
		cssPpi = monitorPpi / dpr;

		metrics.push("<div>Mode: <code>monitor specs + DPR</code></div>");
		metrics.push(
			`<div>Monitor: <code>${monitorWidth}x${monitorHeight}</code> physical px, DPR <code>${dpr.toFixed(2)}</code></div>`,
		);
		metrics.push(
			`<div>Monitor PPI: <code>${monitorPpi.toFixed(2)}</code></div>`,
		);
	} else {
		const measuredMm = parsePositive(fields.calibrationMeasuredMm.value);
		const barPx = calibrationBarPixels();

		if (barPx === null || measuredMm === null) {
			fields.error.textContent =
				"Ruler mode needs bar length and measured millimeters.";
			return;
		}

		cssPpi = (barPx * 25.4) / measuredMm;

		metrics.push("<div>Mode: <code>ruler calibration</code></div>");
		metrics.push(
			`<div>Calibrated CSS PPI: <code>${cssPpi.toFixed(2)}</code></div>`,
		);
		metrics.push(
			`<div>Calibration input: ${barPx.toFixed(0)} CSS px measured as ${measuredMm.toFixed(1)} mm</div>`,
		);
	}

	if (cssPpi === null || !Number.isFinite(cssPpi) || cssPpi <= 0) {
		fields.error.textContent = "Could not compute a valid scale.";
		return;
	}

	const zoomValue = fields.zoomSelect.value;
	const fitZoom = Math.min(computeFitZoom(cssPpi), 1);
	const zoom = zoomValue === "fit" ? fitZoom : (parsePositive(zoomValue) ?? 1);

	const fitOption = fields.zoomSelect.querySelector('option[value="fit"]');
	if (fitOption) {
		fitOption.textContent = `Restrict to fit (${(fitZoom * 100).toFixed(0)}%)`;
	}

	const missingProfiles = renderScaledImages(cssPpi, zoom);
	if (missingProfiles.length > 0) {
		fields.error.textContent = `Missing device mapping for board(s): ${missingProfiles.join(", ")}.`;
	}

	metrics.push(
		`<div>Applied CSS PPI: <code>${cssPpi.toFixed(2)}</code> (scaled per board)</div>`,
	);
	metrics.push(
		`<div>Zoom: <code>${(zoom * 100).toFixed(0)}%</code>${zoomValue === "fit" ? " (restrict to fit)" : ""}</div>`,
	);
	metrics.push(
		"<div>Tip: keep browser zoom at 100% while comparing to hardware.</div>",
	);
	fields.metrics.innerHTML = metrics.join("");
}

function bind() {
	const scaleInputs = [
		fields.scaleMode,
		fields.monitorDiagonal,
		fields.calibrationBarPx,
		fields.calibrationMeasuredMm,
	];

	for (const field of scaleInputs) {
		field.addEventListener("input", updateScale);
		field.addEventListener("change", updateScale);
	}

	fields.zoomSelect.addEventListener("change", updateScale);

	fields.toggleSidebar.addEventListener("click", () => {
		const collapsed = !document.body.classList.contains("settings-collapsed");
		setSidebarCollapsed(collapsed);
		updateScale();
	});

	window.addEventListener("resize", updateScale);
}

async function init() {
	try {
		await loadData();
		setDefaults();
		setSidebarCollapsed(readStoredSidebarCollapsed(), false);
		configureBoardMode();
		applyModeVisibility();
		buildScreens();
		bind();
		updateScale();
	} catch (error) {
		fields.error.textContent = String(error);
	}
}

void init();
