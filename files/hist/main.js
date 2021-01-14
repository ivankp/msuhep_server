function str_bool(s) {
  switch (s.toLowerCase()) {
    case "false":
    case "no":
    case "0":
    case "": return false;
    default: return true;
  }
}

var color_hist;
const colors = [
// https://mokole.com/palette.html
'#000080','#FF0000','#006400','#FFA500','#C71585','#778899','#00FF00','#000000',
'#FFFF00','#00FA9A','#00FFFF','#0000FF','#FF00FF','#1E90FF','#FA8072','#EEE8AA',
'#47260F'
];
const set_colors = { };
const plots = [{
  width: 700*0.95, height: 500*0.95,
  logy: false, nice: false,
  ymin: null, ymax: null,
  assign: function(o){
    for (const key in o) this[key] = o[key];
    return this;
  },
  draw: single_plot
}];
plots.push(Object.assign({},plots[0]));

function draw(req,resp) {
  const div = $('#plots > *');
  if (resp.length==0) {
    $("#nodata").show();
    div.hide();
    return;
  } else {
    $("#nodata").hide();
  }

  let hists = [ ];
  for (let h=0, nh=resp.length; h<nh; ++h) {
    const r = resp[h];

    const xs = r[0];
    const ys = r[1].filter((x,i) => !(i%2));
    const us = r[1].filter((x,i) =>  (i%2));

    let hist = [ ];
    for (let i=1, n=xs.length; i<n; ++i) {
      const x1 = xs[i-1], x2 = xs[i], dx = x2 - x1;
      hist.push([ x1, x2, ys[i]/dx, us[i]/dx ]);
    }

    hists.push({ name: r[2], hist: hist });
  }

  const xtitle = req.labels.var1.join(', ');

  plots[0].ymin = null;
  plots[0].ymax = null;
  plots[0].assign({
    hists: hists, div: div[0], x: xtitle, y: 'dσ/dx, pb/[x]'
  }).draw();
  $(div[0]).show();

  if (hists.length<2) {
    $(div[1]).hide();
    return;
  }

  const h0 = hists[0].hist;
  const rats = hists.slice(1).map(h => ({
    name: h.name,
    hist: h.hist.map((b,i) => {
      const b0 = h0[i];
      if (b0===undefined || b[0]!=b0[0] || b[1]!=b0[1])
        return [ b[0], b[1], 0, 0 ];
      let rat = b[2]/b0[2];
      if (!(rat < 1e10)) rat = 0;
      return [ b[0], b[1], rat, rat * Math.hypot(b[3]/b[2], b0[3]/b0[2]) ];
    })
  }));

  plots[1].ymin = null;
  plots[1].ymax = null;
  plots[1].assign({
    hists: rats, div: div[1], x: xtitle, y: 'ratio'
  }).draw();
  $(div[1]).show();
}

function single_plot() {
  const plot = new Plot(this.div,this.width,this.height,'white');

  const yrange =
    plot.hist_yrange(function*(){
      for (const h of this.hists)
        for (const b of h.hist) yield b[2];
    }.call(this),this.logy);

  if (this.ymin!==null) yrange[0] = this.ymin;
  else this.ymin = yrange[0];
  if (this.ymax!==null) yrange[1] = this.ymax;
  else this.ymax = yrange[1];

  plot.axes(
    { range: d3.extent(this.hists.reduce((a,h) => {
        a.push(h.hist[0][0]);
        a.push(h.hist[h.hist.length-1][1]);
        return a;
      },[])), padding: [35,10], label: this.x },
    { range: yrange, padding: [45,5], label: this.y,
      nice: this.nice, log: this.logy }
  );

  const used_colors = [ ];
  this.hists.forEach((h,i) => {
    let color = null;
    if (h.name in set_colors) {
      color = set_colors[h.name];
      if (used_colors.includes(color)) color = null;
      else used_colors.push(color);
    }
    if (color==null) {
      color = colors.find(x => !used_colors.includes(x));
      if (color) {
        used_colors.push(color);
        set_colors[h.name] = color;
      } else color = '#000000';
    }
    h.g = plot.hist(h.hist).attrs({
      stroke: color,
      'stroke-width': 2
    });
  });

  if (this.hists.length > 1) {
    const g = plot.svg.append('g');
    g.selectAll('text').data(this.hists).enter()
    .append('text').text(h => h.name).attrs((h,i) => ({
      x: 0,
      y: 15*i,
      'class': 'plot_legend',
      fill: h.g.attr('stroke')
    })).on('click',function(h,i){
      color_hist = [this,h];
      const leg = $(this);
      const offset = leg.offset();
      $('#color_picker').css({
        top: (offset.top)+'px',
        left: (offset.left+g.node().getBBox().width)+'px'
      })
      .show()
      .find('> input')[0].jscolor.fromString(leg.attr('fill'));
    });
    g.attrs({
      'transform': 'translate('+
        (plot.width-g.node().getBBox().width-5)+',15)',
      'text-anchor': 'start',
      'font-family': 'sans-serif',
      'font-size': '15px'
    });
  }
}

const dummy_a = document.createElement('a');
function save_json(plot) {
  const hists = plot.hists.reduce((m,h) => {
    m[h.name] = h.hist;
    return m;
  },{});
  let json = '{';
  let d1 = true;
  for (const key in hists) {
    if (d1) d1 = false; else json += ',';
    json += '\n\"' + key + '\":[';
    let d2 = true;
    for (const bin of hists[key]) {
      if (d2) d2 = false; else json += ',';
      json += '\n[' + bin.join(',') + ']';
    }
    json += '\n]';
  }
  json += '\n}';

  dummy_a.href = URL.createObjectURL(
    new Blob( [json], {type:'application/json'} )
  );
  dummy_a.download = 'hists.json';
  dummy_a.dispatchEvent(new MouseEvent('click'));
}
function save_svg(plot) {
  dummy_a.href = URL.createObjectURL(new Blob(
    [ '<?xml version="1.0" encoding="UTF-8" ?>\n',
      plot.div.getElementsByTagName('svg')[0].outerHTML ],
    { type:"image/svg+xml;charset=utf-8" }
  ));
  dummy_a.download = 'plot.svg';
  dummy_a.dispatchEvent(new MouseEvent('click'));
}

$(function() {
  DBView({
    div: $('#dbview'),
    dbs: dbs,
    default_selection: (col,i) => (i==0 && !/^var\d+$/.test(col)),
    process_data: draw
  });

  let logy1 = window.location.href.match(/.*[?&](logy)(?:&|$|=([^&]+))/i);
  if (logy1) {
    logy1 = logy1[2];
    if (logy1) logy1 = str_bool(logy1);
    else logy1 = true;
  } else {
    logy1 = false;
  }
  plots[0].logy = logy1;

  $('#color_picker > input').change(function(){
    const color = this.value;
    color_hist[0].setAttribute('fill',color);
    color_hist[1].g.attr('stroke',color);
    set_colors[color_hist[1].name] = color;
  }).focusout(function(){
    $('#color_picker').hide();
  });

  function parse_flt(text) {
    let x = parseFloat(text);
    if (isNaN) return null;
    return x;
  }

  // https://swisnl.github.io/jQuery-contextMenu/docs.html
  $.contextMenu({
    selector: '#plots > *',
    items: {
      logy: {
        name: 'log y scale',
        type: 'checkbox',
        selected: false,
        events: { click: function(e) {
          const plot = plots[e.data.$trigger.index()];
          plot.logy = !plot.logy;
          plot.ymin = null;
          plot.ymax = null;
          plot.draw();
          const items = e.data.items;
          items.ymin.$input.val(items.ymin.value = plot.ymin);
          items.ymax.$input.val(items.ymax.value = plot.ymax);
        } }
      },
      // nice: {name: 'nice y range'},
      sep1: "---------",
      save_json: {name: 'save as JSON'},
      save_svg: {name: 'save as SVG'},
      sep2: "---------",
      ymin: {
        name: "Ymin",
        type: 'text',
        events: { keyup: function(e) { if (e.keyCode === 13) {
          const plot = plots[e.data.$trigger.index()];
          const item = e.data.items.ymin;
          const y = parseFloat(this.value);
          plot.ymin = isNaN(y) ? null : y;
          plot.draw();
          item.$input.val(item.value = plot.ymin);
        } } }
      },
      ymax: {
        name: "Ymax",
        type: 'text',
        events: { keyup: function(e) { if (e.keyCode === 13) {
          const plot = plots[e.data.$trigger.index()];
          const item = e.data.items.ymax;
          const y = parseFloat(this.value);
          plot.ymax = isNaN(y) ? null : y;
          plot.draw();
          item.$input.val(item.value = plot.ymax);
        } } }
      },
    },
    callback: function(key, options) {
      const plot = plots[this.index()];
      switch (key) {
        case 'save_json':
        case 'save_svg':
          window[key](plot);
          break;
      }
    },
    events: {
      show: function(options) {
        const plot = plots[this.index()];
        options.items.logy.selected = plot.logy;
        options.items.ymin.value = plot.ymin || '0';
        options.items.ymax.value = plot.ymax || '0';
      }
    }
  });

  $('#show_notes').click(function(){
    const notes = $('#notes');
    const hidden = notes.is(':hidden');
    $(this).text('[' + (hidden ? 'hide' : 'show') + ' notes]');
    notes.slideToggle('fast');
  });
});
