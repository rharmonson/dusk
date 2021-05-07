/* Compile-time check to make sure that the number of bar rules do not exceed the limit */
struct NumBarRules { char TooManyBarRules__Increase_BARRULES_macro_to_fix_this[LENGTH(barrules) > BARRULES ? -1 : 1]; };

void
barpress(XButtonPressedEvent *ev, Monitor *m, Arg *arg, int *click)
{
	Bar *bar;
	const BarRule *br;
	BarArg barg = { 0, 0, 0, 0 };
	int barclick, r;

	for (bar = selmon->bar; bar; bar = bar->next) {
		if (ev->window == bar->win) {
			for (r = 0; r < LENGTH(barrules); r++) {
				br = &barrules[r];
				if (br->bar != bar->idx || (br->monitor == 'A' && m != selmon) || br->clickfunc == NULL)
					continue;
				if (br->monitor != 'A' && br->monitor != -1 && br->monitor != bar->mon->num)
					continue;
				if (bar->vert && (bar->p[r] > ev->y || ev->y > bar->p[r] + bar->s[r]))
					continue;
				if (!bar->vert && (bar->p[r] > ev->x || ev->x > bar->p[r] + bar->s[r]))
					continue;

				if (bar->vert) {
					barg.x = ev->y - bar->borderpx;
					barg.y = ev->x - (bar->p[r] + br->lpad);
					barg.w = bar->bw - 2 * bar->borderpx;
					barg.h = bar->s[r];;
				} else {
					barg.x = ev->x - (bar->p[r] + br->lpad);
					barg.y = ev->y - bar->borderpx;
					barg.w = bar->s[r];
					barg.h = bar->bh - 2 * bar->borderpx;
				}
				barg.lpad = br->lpad;
				barg.rpad = br->rpad;
				barg.value = br->value;

				barclick = br->clickfunc(bar, arg, &barg);
				if (barclick > -1)
					*click = barclick;
				return;
			}
			break;
		}
	}
}

void
createbars(Monitor *m)
{
	const BarDef *def;

	for (int i = 0; i < LENGTH(bars); i++) {
		def = &bars[i];
		if (def->monitor == m->num)
			createbar(def, m);
	}
}

void
createbar(const BarDef *def, Monitor *m)
{
	Bar *bar;
	bar = ecalloc(1, sizeof(Bar));
	bar->mon = m;
	bar->idx = def->idx;
	bar->next = m->bar;
	bar->name = def->name;
	bar->vert = def->vert;
	bar->barpos = def->barpos;
	bar->showbar = 1;
	bar->external = 0;
	bar->borderpx = enabled(BarBorder) ? borderpx : 0;
	m->bar = bar;
}

void
drawbar(Monitor *m)
{
	Bar *bar;
	for (bar = m->bar; bar; bar = bar->next)
		drawbarwin(bar);
}

void
drawbars(void)
{
	Monitor *m;
	for (m = mons; m; m = m->next)
		drawbar(m);
}

void
drawbarwin(Bar *bar)
{
	if (!bar || !bar->win || bar->external)
		return;

	int r, w, mw, total_drawn = 0, groupactive, ignored;
	int rx, lx, rw, lw; // bar size, split between left and right if a center module is added
	const BarRule *br;
	Monitor *lastmon;

	if (enabled(BarActiveGroupBorderColor))
		getclientcounts(bar->mon->selws, &groupactive, &ignored, &ignored, &ignored, &ignored, &ignored, &ignored);
	else
		groupactive = GRP_MASTER; // TOOO, hmm, dependency on flexwintitle?
	bar->scheme = getschemefor(bar->mon->selws, groupactive, bar->mon == selmon);

	if (bar->borderpx) {
		XSetForeground(drw->dpy, drw->gc, scheme[bar->scheme][ColBorder].pixel);
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, 0, 0, bar->bw, bar->bh);
	}

	BarArg barg = { 0 };
	barg.h = bar->bh - 2 * bar->borderpx;

	rw = lw = (bar->vert ? bar->bh : bar->bw) - 2 * bar->borderpx;
	rx = lx = bar->borderpx;

	for (lastmon = mons; lastmon && lastmon->next; lastmon = lastmon->next);

	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_rect(drw, lx, bar->borderpx, lw, bar->bh - 2 * bar->borderpx, 1, 1);

	for (r = 0; r < LENGTH(barrules); r++) {
		br = &barrules[r];
		if (br->bar != bar->idx || !br->sizefunc || (br->monitor == 'A' && bar->mon != selmon))
			continue;
		if (br->monitor != 'A' && br->monitor != -1 && br->monitor != bar->mon->num &&
				!(br->drawfunc == draw_systray && br->monitor > lastmon->num && bar->mon->num == 0)) // hack: draw systray on first monitor if the designated one is not available
			continue;
		if (br->scheme != -1)
			drw_setscheme(drw, scheme[br->scheme]);
		barg.lpad = br->lpad;
		barg.rpad = br->rpad;
		barg.value = br->value;

		mw = (br->alignment < BAR_ALIGN_RIGHT_LEFT ? lw : rw);
		barg.w = MAX(0, mw - br->lpad);
		w = br->sizefunc(bar, &barg);
		barg.w = w = MIN(barg.w, w);

		if (w) {
			w += br->lpad;
			if (w + br->rpad <= mw)
				w += br->rpad;
		}
		bar->s[r] = w;

		if (lw <= 0) { // if left is exhausted then switch to right side, and vice versa
			lw = rw;
			lx = rx;
		} else if (rw <= 0) {
			rw = lw;
			rx = lx;
		}

		switch (br->alignment) {
		default:
		case BAR_ALIGN_NONE:
		case BAR_ALIGN_TOP_TOP:
		case BAR_ALIGN_LEFT_LEFT:
		case BAR_ALIGN_TOP:
		case BAR_ALIGN_LEFT:
			bar->p[r] = lx;
			if (lx == rx) {
				rx += w;
				rw -= w;
			}
			lx += w;
			lw -= w;
			break;
		case BAR_ALIGN_TOP_BOTTOM:
		case BAR_ALIGN_LEFT_RIGHT:
		case BAR_ALIGN_BOTTOM:
		case BAR_ALIGN_RIGHT:
			bar->p[r] = lx + lw - w;
			if (lx == rx)
				rw -= w;
			lw -= w;
			break;
		case BAR_ALIGN_TOP_CENTER:
		case BAR_ALIGN_LEFT_CENTER:
		case BAR_ALIGN_CENTER:
			bar->p[r] = lx + lw / 2 - w / 2;
			if (lx == rx) {
				rw = rx + rw - bar->p[r] - w;
				rx = bar->p[r] + w;
			}
			lw = bar->p[r] - lx;
			break;
		case BAR_ALIGN_BOTTOM_TOP:
		case BAR_ALIGN_RIGHT_LEFT:
			bar->p[r] = rx;
			if (lx == rx) {
				lx += w;
				lw -= w;
			}
			rx += w;
			rw -= w;
			break;
		case BAR_ALIGN_BOTTOM_BOTTOM:
		case BAR_ALIGN_RIGHT_RIGHT:
			bar->p[r] = rx + rw - w;
			if (lx == rx)
				lw -= w;
			rw -= w;
			break;
		case BAR_ALIGN_BOTTOM_CENTER:
		case BAR_ALIGN_RIGHT_CENTER:
			bar->p[r] = rx + rw / 2 - w / 2;
			if (lx == rx) {
				lw = lx + lw - bar->p[r] + w;
				lx = bar->p[r] + w;
			}
			rw = bar->p[r] - rx;
			break;
		}
		if (bar->vert) {
			barg.x = bar->borderpx + 5;
			barg.y = bar->p[r] + br->lpad;
			barg.h = barg.w;
			barg.w = bar->bw - 2 * bar->borderpx;
		} else {
			barg.x = bar->p[r] + br->lpad;
			barg.y = bar->borderpx;
			barg.h = bar->bh - 2 * bar->borderpx;
		}

		if (br->drawfunc)
			total_drawn += br->drawfunc(bar, &barg);
	}

	if (total_drawn == 0 && bar->showbar) {
		bar->showbar = 0;
		updatebarpos(bar->mon);
		XMoveResizeWindow(dpy, bar->win, bar->bx, bar->by, bar->bw, bar->bh);
		setworkspaceareasformon(bar->mon);
		arrangemon(bar->mon);
	}
	else if (total_drawn > 0 && !bar->showbar) {
		bar->showbar = 1;
		updatebarpos(bar->mon);
		XMoveResizeWindow(dpy, bar->win, bar->bx, bar->by, bar->bw, bar->bh);
		drw_map(drw, bar->win, 0, 0, bar->bw, bar->bh);
		setworkspaceareasformon(bar->mon);
		arrangemon(bar->mon);
	} else
		drw_map(drw, bar->win, 0, 0, bar->bw, bar->bh);
}

void
updatebarpos(Monitor *m)
{
	Bar *bar;
	for (bar = m->bar; bar; bar = bar->next)
		setbarpos(bar);
	reducewindowarea(m);
}

void
setbarpos(Bar *bar)
{
	char xCh, yCh, wCh, hCh;
	float w, h;
	float x, y;

	int y_pad = (enabled(BarPadding) ? vertpad : 0);
	int x_pad = (enabled(BarPadding) ? sidepad : 0);
	Monitor *m = bar->mon;

	switch (sscanf(bar->barpos, "%f%c %f%c %f%c %f%c", &x, &xCh, &y, &yCh, &w, &wCh, &h, &hCh)) {
	case 8:
		// all good
		break;
	default:
		fprintf(stderr, "Bar %s (%d) on monitor %d, bad barpos '%s' - can't place bar\n", bar->name, bar->idx, bar->mon->num, bar->barpos);
		return;
	}

	bar->bx = m->mx + x_pad;
	bar->by = m->my + y_pad;
	if (bar->vert) {
		bar->bh = m->mh - 2 * y_pad;
		bar->bw = bh + bar->borderpx * 2;
	} else {
		bar->bh = bh + bar->borderpx * 2;
		bar->bw = m->mw - 2 * x_pad;
	}

	if (wCh == '%') {
		bar->bw = (m->mw - 2 * x_pad) * w / 100;
	} else if (wCh == 'w') {
		if (w > 0)
			bar->bw = w;
	}
	if (hCh == '%') {
		bar->bh = (m->mh - 2 * y_pad) * h / 100;
	} else if (hCh == 'h') {
		if (h > 0)
			bar->bh = h;
	}

	if (xCh == '%') {
		bar->bx += (m->mw - 2 * x_pad - bar->bw) * x / 100;
	} else if (xCh == 'x') {
		if (x >= 0)
			bar->bx = m->mx + x;
	}

	if (yCh == '%') {
		bar->by += (m->mh - 2 * y_pad - bar->bh) * y / 100;
	} else if (yCh == 'y') {
		if (y >= 0)
			bar->by = m->my + y;
	}

	if (!bar->showbar || !m->showbar) {
		if (bar->vert) {
			bar->bx = -bar->bw;
		} else {
			bar->by = -bar->bh;
		}
	}
}

void
reducewindowarea(Monitor *m)
{
	Bar *bar;

	m->wx = m->mx;
	m->wy = m->my;
	m->ww = m->mw;
	m->wh = m->mh;

	for (bar = m->bar; bar; bar = bar->next) {
		if (bar->vert) { // vertical bar
			if (bar->bx < m->mw / 2) { // left aligned
				if (m->wx < bar->bx + bar->bw) {
					m->ww -= (bar->bx + bar->bw - m->wx);
					m->wx = bar->bx + bar->bw;
				}
			} else { // right aligned
				if (bar->bx < m->wx + m->ww) {
					m->ww = bar->bx - m->wx;
				}
			}
		} else { // horizontal bar
			if (bar->by < m->mh / 2) { // top bar
				if (m->wy < bar->by + bar->bh) {
					m->wh -= (bar->by + bar->bh - m->wy);
					m->wy = bar->by + bar->bh;
				}
			} else { // bottom bar
				if (bar->by < m->wy + m->wh) {
					m->wh = bar->by - m->wy;
				}
			}
		}
	}
}

void
updatebars(void)
{
	Bar *bar;
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixel = 0,
		.border_pixel = 0,
		.colormap = cmap,
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"dusk", "dusk"};
	for (m = mons; m; m = m->next) {
		for (bar = m->bar; bar; bar = bar->next) {
			if (bar->external)
				continue;
			if (!bar->win) {
				bar->win = XCreateWindow(dpy, root, bar->bx, bar->by, bar->bw, bar->bh, 0, depth,
				                          InputOutput, visual,
				                          CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
				XDefineCursor(dpy, bar->win, cursor[CurNormal]->cursor);
				XMapRaised(dpy, bar->win);
				XSetClassHint(dpy, bar->win, &ch);
			}
		}
	}
}

void
togglebar(const Arg *arg)
{
	Bar *bar;
	selmon->showbar = !selmon->showbar;
	updatebarpos(selmon);
	for (bar = selmon->bar; bar; bar = bar->next)
		XMoveResizeWindow(dpy, bar->win, bar->bx, bar->by, bar->bw, bar->bh);
	setworkspaceareasformon(selmon);
	arrangemon(selmon);
}

void
togglebarpadding(const Arg *arg)
{
	Bar *bar;
	togglefunc(BarPadding);
	for (Monitor *m = mons; m; m = m->next) {
		updatebarpos(m);
		for (bar = m->bar; bar; bar = bar->next)
			XMoveResizeWindow(dpy, bar->win, bar->bx, bar->by, bar->bw, bar->bh);
		setworkspaceareasformon(m);
		drawbar(m);
		arrangemon(m);
	}
}