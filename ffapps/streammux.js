/**
 * \verbatim
 * flipflip's c++ apps (ffapps)
 *
 * Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
 * https://oinkzwurgl.org/projaeggd/ffxx/
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 * \endverbatim
 *
 * @file
 * @brief stream multiplexer
 */
/* ****************************************************************************************************************** */

"use strict";

// ---------------------------------------------------------------------------------------------------------------------

class SmBase
{
    debug_name = "SmBase";
    debug_enabled = false;

    constructor(debug, debug_name)
    {
        if (debug) {
            this.debug_enabled = true;
        }
        if (debug_name) {
            this.debug_name = debug_name;
        }
    }

    DEBUG(strOrObj, objOrNada) {
        if (window.console && this.debug_enabled) {
            if (objOrNada) { console.log(this.debug_name + ': ' + strOrObj + ': %o', objOrNada); }
            else if (typeof strOrObj === 'object') { console.log(this.debug_name + ': %o', strOrObj); }
            else { console.log(this.debug_name + ': ' + strOrObj); }
        }
    }

    _FmtNum(num, sep="'")
    {
        return String(num).replace(/\.\d+|\d(?=(?:\d{3})+(?!\d))/g, a => a.length == 1 ? a + sep : a);
    }

    _UpdateTextContent(el, data) {
        let str = data;
        if (Array.isArray(data)) {
            str = data.join("\n");
        }

        if (el.textContent !== str) {
            el.textContent = str;
            if (el.classList.contains("sm-tooltip")) {
                el.setAttribute("title", str);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

function SmGET(url, cb)
{
    const abort_ctrl = new AbortController();
    const abort_to = setTimeout(() => abort_ctrl.abort(), 2500);
    fetch(url, { method: "GET", signal: abort_ctrl.signal }).then(async res => {
        clearTimeout(abort_to);
        if (res.ok) {
            cb(await res.json());
        } else {
            cb(false);
        }

    });
}

function SmPOST(url, data, cb)
{
    const abort_ctrl = new AbortController();
    const abort_to = setTimeout(() => abort_ctrl.abort(), 2500);
    fetch(url, {
        method: "POST", body: JSON.stringify(data), headers: { "Content-type": "application/json" }, signal: abort_ctrl.signal
    }).then(async res => {
        clearTimeout(abort_to);
        if (res.ok) {
            cb(await res.json());
        } else {
            cb(false);
        }
    });
}

// ---------------------------------------------------------------------------------------------------------------------

class SmStats extends SmBase
{
    constructor(debug, container, name)
    {
        super(debug, "SmStats(" + name + ")");
        this.container = container;
        this.els = { a: { }, b: { } };
        this.fields = ["n_msgs", "s_msgs", "n_err", "n_filt", "s_filt",
            "n_fpa", "s_fpa", "n_fpa", "s_fpa", "n_nmea", "s_nmea", "n_novb", "s_novb", "n_rtcm3", "s_rtcm3",
            "n_spartn", "s_spartn", "n_ubx", "s_ubx", "n_unib", "s_unib", "n_other", "s_other",];
        for (const f of this.fields) {
            this.els.a[f] = this.container.querySelector(".sm-data-stats_a-" + f);
            this.els.b[f] = this.container.querySelector(".sm-data-stats_b-" + f);
        }
        this.statsa = undefined;
        this.statsb = undefined;
        this.relative = false;

        this.DEBUG(this);
    }

    Update(stats)
    {
        // this.DEBUG("Update", [ statsa, statsb ]);
        let statsa = stats[0];
        let statsb = stats[1];
        for (const f of this.fields) {
            const vala = (this.relative && this.statsa ? statsa[f] - this.statsa[f] : statsa[f]);
            const valb = (this.relative && this.statsb ? statsb[f] - this.statsb[f] : statsb[f]);
            this.els.a[f].textContent = this._FmtNum(vala);
            this.els.b[f].textContent = this._FmtNum(valb);
            this.els.a[f].parentElement.classList.toggle("sm-dim", !vala && !valb);
        }

        this.statsa = statsa;
        this.statsb = statsb;
    }

    ToggleRelative(relative)
    {
        this.DEBUG("ToggleRelative " + relative);
        this.relative = relative;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

class SmProcStatus extends SmBase
{
    constructor(debug, container)
    {
        super(debug, "SmProcStatus");
        this.container = container;
        this.els = { };
        this.fields = [ "mem_curr", "mem_peak", "cpu_curr", "cpu_avg", "cpu_peak", "pid", "time", "uptime" ];
        for (const f of this.fields) {
            this.els[f] = this.container.querySelector(".sm-data-" + f);
        }
        this.DEBUG(this);
    }

    Update(proc)
    {
        // this.DEBUG("Update", proc);
        proc.cpu_curr = proc.cpu_curr.toFixed(1);
        proc.cpu_avg = proc.cpu_avg.toFixed(1);
        proc.cpu_peak = proc.cpu_peak.toFixed(1);
        proc.mem_curr = proc.mem_curr.toFixed(1);
        proc.mem_peak = proc.mem_peak.toFixed(1);
        proc.time = proc.time.replace(/^([0-9]{4,4})([0-9]{2,2})([0-9]{2,2})T([0-9]{2,2})([0-9]{2,2})([0-9]{2,2})Z$/, "$1-$2-$3 $4:$5:$6 UTC")
        for (const f of this.fields) {
            if (this.els[f].textContent !== proc[f]) {
                this.els[f].textContent = proc[f];
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

class SmActionToggle extends SmBase
{
    constructor(debug, target, callback)
    {
        super(debug, "SmActionToggle");
        this.target = target;
        this.icons = this.target.querySelectorAll(".sm-action-icon"),
        this.callback = callback;
        this.Set(false);
        this.target.addEventListener("click", this._Toggle.bind(this));
    }

    Set(state)
    {
        this.state = state;
        this.icons[0].classList.toggle("sm-action-icon-hide", this.state);
        this.icons[1].classList.toggle("sm-action-icon-hide", !this.state);
        if (this.callback) {
            this.callback(this.state, event);
        }
    }

    _Toggle(event)
    {
        // this.DEBUG("Toggle", event);
        this.Set(!this.state);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

class SmActionOnOff extends SmBase
{
    constructor(debug, debug_name, target, callback)
    {
        super(debug, "SmActionOnOff(" + debug_name + ")");
        this.target = target;
        this.callback = callback;
        this.icons = this.target.querySelectorAll(".sm-action-icon"); // dis, on, off
        this.Update(false, false, 0);
        this.target.addEventListener("click", this._Click.bind(this));
    }

    Update(can, ena, n_msgs)
    {
        // Update icons
        this.icons[0].classList.toggle("sm-action-icon-hide", can);             // dis
        this.icons[1].classList.toggle("sm-action-icon-hide", !can || !ena);    // on
        this.icons[2].classList.toggle("sm-action-icon-hide", !can || ena);     // off
        // Blink?
        clearTimeout(this.blink_to);
        if (this.n_msgs && ((n_msgs - this.n_msgs) !== 0)) {
            this.blink_to = setTimeout(() => { this.icons[1].style.animationPlayState = "paused"; }, 2500);
            this.icons[1].style.animationPlayState = "running";
            const dur = (1.0 / Math.min(Math.max((n_msgs - this.n_msgs) / 5, 1), 20)).toFixed(1) + "s";
            if (dur !== this.blink_dur) {
                this.icons[1].style.animationDuration = dur;
                this.blink_dur = dur;
            }
        } else if (this.icons[1].style.animationPlayState !== "paused") {
            this.icons[1].style.animationPlayState = "paused";
        }
        this.n_msgs = n_msgs;
        this.can = can;
        this.ena = ena;
    }

    _Click(event)
    {
        this.DEBUG("Click", event);
        this.state = !this.state;
        if (this.can && this.callback) {
            this.callback(this, event);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

class SmStatusBase extends SmBase
{
    constructor(debug, debug_name, container, status, ws, fields)
    {
        super(debug, debug_name, container);
        this.container = container;
        this.details_els = this.container.querySelectorAll('.sm-details');
        this.blocker = this.container.querySelector('.sm-blocker');
        this.ws = ws;
        this.fields = fields;
        this.els = { };
        for (const f of this.fields) {
            this.els[f] = this.container.querySelector(".sm-data-" + f);
        }
        this.stats = new SmStats(debug, container, status.name);
        this.onoff0 = new SmActionOnOff(debug, status.name, this.container.querySelector(".sm-action-ena0"), this._ToggleOnOff.bind(this, 0));
        this.onoff1 = new SmActionOnOff(debug, status.name, this.container.querySelector(".sm-action-ena1"), this._ToggleOnOff.bind(this, 1));
        this.UpdateStatus(status);

        this.details_tog = new SmActionToggle(
            debug, this.container.querySelector(".sm-action-details"), this._ToggleDetails.bind(this));
        this.stats_tog = new SmActionToggle(
            debug, this.container.querySelector(".sm-action-relative"), this._ToggleStats.bind(this));

        this.DEBUG(this);
    }

    UpdateStatus(status)
    {
        this.status = status;
        // this.DEBUG("Update", str);
        status.filter_a = status.filter[0];
        status.filter_b = status.filter[1];
        for (const f of this.fields) {
            this._UpdateTextContent(this.els[f], status[f]);
        }

        this.stats.Update(status.stats);
        this.onoff0.Update(status.can[0], status.ena[0], status.stats[0].n_msgs);
        this.onoff1.Update(status.can[1], status.ena[1], status.stats[1].n_msgs);
    }

    UpdateEna(ena0, ena1)
    {
        if (this.status) {
            this.onoff0.Update(this.status.can[0], ena0, this.status.stats[0].n_msgs);
            this.onoff1.Update(this.status.can[1], ena1, this.status.stats[1].n_msgs);
        }
        this.Blocker(false);
    }

    Blocker(show)
    {
        this.blocker.classList.toggle("sm-blocker-show", show);
    }

    _ToggleDetails(state, event)
    {
        this.details_els.forEach(el => { el.classList.toggle("sm-details-show", state); });
    }

    _ToggleStats(state, event)
    {
        this.stats.ToggleRelative(state);
    }

    _ToggleOnOff(ix, onoff, event)
    {
        this.DEBUG("ToggleOnOff " + ix + ' ' + onoff.ena);
        this.Blocker(true);
        let ctrl = [ this.status.name, null, null ];
        ctrl[1 + ix] = !onoff.ena;
        this.ws.send(JSON.stringify({ api: "ctrl", data: ctrl }));
    }
}

// ---------------------------------------------------------------------------------------------------------------------

class SmStrStatus extends SmStatusBase
{
    constructor(debug, container, status, ws)
    {
        super(debug, "SmStrStatus(" + status.name + ")", container, status, ws,
            [ "name", "type", "mode", "state", "error", "opts", "info", "disp", "statestrs", "statestrs2", "filter_a", "filter_b" ]);
    }

    UpdateStatus(status)
    {
        status.statestrs2 = status.statestrs;
        status.opts = status.opts.substr( 1 + status.opts.substr(1).indexOf(",") + 1);
        super.UpdateStatus(status);
    }

}

// ---------------------------------------------------------------------------------------------------------------------

class SmMuxStatus extends SmStatusBase
{
    constructor(debug, container, status, ws)
    {
        super(debug, "SmMuxStatus(" + status.name + ")", container, status, ws,
            [ "name", "src", "dst", "filter_a", "filter_b" ]);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

class StreamMux extends SmBase {
    constructor(container, url, debug)
    {
        super(debug, "StreamMux");
        this.debug = debug;
        this.container = container;
        this.url = url;
        this.els = {
            ws_status:            this.container.querySelector(".sm-ws-status"),
            status_proc:          this.container.querySelector(".sm-status-proc"),
            footer_copyright:     this.container.querySelector(".sm-footer-copyright"),
            proc_status:          this.container.querySelector(".sm-status-proc"),
            str_status_templ:     this.container.querySelector(".sm-templates .sm-status-str"),
            mux_status_templ:     this.container.querySelector(".sm-templates .sm-status-mux"),
            stats_templ:          this.container.querySelector(".sm-templates .sm-stats"),
            str_status_container: this.container.querySelector(".sm-str-status-container"),
            mux_status_container: this.container.querySelector(".sm-mux-status-container"),
        };
        this.UpdateWsStatus(this.#WS_DISCONNECTED);

        this.proc = new SmProcStatus(this.debug, this.els.proc_status);
        this.strs = {}; // SmStrStatus
        this.muxs = {}; // SmMuxStatus

        this.allrelative_tog = new SmActionToggle(
            debug, this.container.querySelector(".sm-action-allrelative"), this._ToggleAllRelative.bind(this));
        this.alldetails_tog = new SmActionToggle(
                debug, this.container.querySelector(".sm-action-alldetails"), this._ToggleAllDetails.bind(this));

        this.DEBUG(this);
    }

    Start()
    {
        this.DEBUG("Start");
        let ok = true;

        let versionurl = new URL("/version", this.url);
        SmGET(versionurl, this.OnVersion.bind(this));

        let wsurl = new URL("/ws", this.url);
        wsurl.protocol = wsurl.protocol.replace('http', 'ws');
        try {
            this.ws = new WebSocket(wsurl);
            this.ws.addEventListener("message", this.OnWsMessage.bind(this));
            this.ws.addEventListener("open", this.OnWsOpen.bind(this));
        }
        catch (error) {
            console.error(error);
            ok = false;
        }

        return ok;
    }

    OnVersion(data)
    {
        this.DEBUG("OnVersion", data);
        this._UpdateTextContent(this.els.footer_copyright, "Version: " + data.version + ", " + data.copyright + ", " + data.license);
    }

    OnWsOpen(event)
    {
        this.DEBUG("OnWsOpen", event);
        this.UpdateWsStatus(this.#WS_CONNECTING);
    }

    OnWsMessage(event)
    {
        let message = undefined;
        try {
            message = JSON.parse(event.data);
            this.DEBUG("OnWsMessage", message);
            this.UpdateWsStatus(this.#WS_CONNECTED);
        } catch (error) {
            this.DEBUG("OnWsMessage", [ error, event ]);
        }
        if (message && message.api) {
            if (message.api === "status") {
                this.UpdateUiStatus(message);
            } else if (message.api === "ctrl") {
                this.UpdateUiCtrl(message.data)
            }
        }
    }

    UpdateUiStatus(status)
    {
        for (const str_status of status.strs) {
            if (!(str_status.name in this.strs)) {
                let templ = this.els.str_status_templ.cloneNode(true);
                templ.appendChild( this.els.stats_templ.cloneNode(true) );
                this.els.str_status_container.appendChild(templ);
                this.strs[str_status.name] = new SmStrStatus(this.debug, templ, str_status, this.ws);
                this.SetupLink(this.strs[str_status.name]);
            }
            this.strs[str_status.name].UpdateStatus(str_status);
        }

        for (const mux_status of status.muxs) {
            if (!(mux_status.name in this.muxs)) {
                let templ = this.els.mux_status_templ.cloneNode(true);
                templ.appendChild( this.els.stats_templ.cloneNode(true) );
                this.els.mux_status_container.appendChild(templ);
                this.muxs[mux_status.name] = new SmMuxStatus(this.debug, templ, mux_status, this.ws);
                this.SetupLink(this.muxs[mux_status.name]);
            }
            this.muxs[mux_status.name].UpdateStatus(mux_status);
        }

        this.proc.Update(status.proc);
    }

    UpdateUiCtrl(ctrl)
    {
        const str_or_mux = ctrl[0];
        if (str_or_mux in this.strs) {
            this.strs[str_or_mux].UpdateEna(ctrl[1], ctrl[2]);
            return;
        }
        if (str_or_mux in this.muxs) {
            this.muxs[str_or_mux].UpdateEna(ctrl[1], ctrl[2]);
            return;
        }
    }

    #WS_DISCONNECTED = 1;
    #WS_CONNECTING = 2;
    #WS_CONNECTED = 3;

    UpdateWsStatus(state)
    {
        let highlight = false;
        switch (state) {
            case this.#WS_DISCONNECTED:
                this._UpdateTextContent(this.els.ws_status, "Disconnected");
                highlight = true;
                break;
            case this.#WS_CONNECTING:
                this._UpdateTextContent(this.els.ws_status, "Connecting");
                highlight = true;
                break;
            case this.#WS_CONNECTED:
                this._UpdateTextContent(this.els.ws_status, "Connected");
                highlight = true;
                break;
        }
        if (highlight) {
            //if (this.ws_status_highlight_to) {
                clearTimeout(this.ws_status_highlight_to);
            //}
            this.els.ws_status.classList.add("sm-highlight-bg");
            this.ws_status_highlight_to = setTimeout(() => { this.els.ws_status.classList.remove("sm-highlight-bg"); }, 100);
        }
    }

    SetupLink(inst)
    {
        inst.container.addEventListener("mouseenter", this.LinkOnMouseEnterLeave.bind(this, inst, true));
        inst.container.addEventListener("mouseleave", this.LinkOnMouseEnterLeave.bind(this, inst, false));
    }

    LinkOnMouseEnterLeave(inst, enter_leave, event)
    {
        this.DEBUG((enter_leave ? "enter" : "leave") + " " + inst.status?.name);
        if (inst.status && !inst.status.src) { // mux
            for (let mux of Object.values(this.muxs)) {
                if ((inst.status.name === mux.status.src) || (inst.status.name === mux.status.dst)) {
                    mux.container.classList.add("sm-status-linked");
                    mux.container.classList.toggle("sm-status-linked", enter_leave);
                }
            }
        }
        else if (inst.status) { // str
            for (let str of Object.values(this.strs)) {
                if ((inst.status.src === str.status.name) || (inst.status.dst === str.status.name)) {
                    str.container.classList.add("sm-status-linked");
                    str.container.classList.toggle("sm-status-linked", enter_leave);
                }
            }
        }
    }

    _ToggleAllDetails(state, event)
    {
        for (let str of Object.values(this.strs)) {
            console.log(str);
            str.details_tog.Set(state);
        }
        for (let mux of Object.values(this.muxs)) {
            mux.details_tog.Set(state);
        }
    }

    _ToggleAllRelative(state, event)
    {
        for (let str of Object.values(this.strs)) {
            console.log(str);
            str.stats_tog.Set(state);
        }
        for (let mux of Object.values(this.muxs)) {
            mux.stats_tog.Set(state);
        }
    }
}

/* ****************************************************************************************************************** */

(function (main) { if (document.readyState !== "loading") { main(); } else { document.addEventListener("DOMContentLoaded", main); } })(() => {
    const debug = (location.href.indexOf("debug=1") > -1);
    if (debug) {
        console.clear();
    }
    let sm = new StreamMux(
        document.querySelector("#streammux"),
        new URL("/", window.location.href),
        debug,
    );
    sm.Start();
   window.sm = sm;
});

/* ****************************************************************************************************************** */
