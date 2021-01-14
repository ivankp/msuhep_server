function Plot(select,width,height,bkg) {

this.width = width;
this.height = height;

this.container = d3.select(select);
this.container.select('svg').remove();
this.svg = this.container.append('svg').attrs({
  width: width,
  height: height,
  version: '1.1',
  xmlns: 'http://www.w3.org/2000/svg'
});
if (bkg!=null) this.svg.append('rect').attrs({
  width: width,
  height: height,
  fill: bkg
});

this.axes = function(xa,ya) {
  this.svg.selectAll('g.axis').remove();
  return this.scales = [xa,ya].map((a,i,as) => {
    let scale = (a.log ? d3.scaleLog() : d3.scaleLinear())
      .domain(a.range)
      .range(i==0
        ? [as[1-i].padding[0],width-a.padding[1]]
        : [height-as[1-i].padding[0],a.padding[1]] )
      .clamp(true);
    if (a.nice) scale = scale.nice();
    let axis = (i==0 ? d3.axisBottom : d3.axisLeft)(scale);
    axis.tickSizeOuter(0);
    // axis.tickFormat(d3.format("~g"));
    if (!a.log) {
      const abs_range = a.range.map(x => {
        if (x<0) x = -x;
        if (0<x && x<1) x = 1./x;
        return x;
      });
      if (Math.max(...abs_range) >= 1e4)
        axis.tickFormat(d3.format("~e"));
    }
    if (a.values) axis.tickValues(a.values);
    let g = this.svg.append('g').attr('class','axis')
       .attr('transform','translate('+(
           i==0 ? [0,height-a.padding[0]] : [a.padding[0],0]
         )+')').call( axis );
    if (a.label) {
      let label = g.append('text')
        .attr('text-anchor','end')
        .attr('fill', '#000')
        .html(a.label);
      if (i==0) label.attr('x',width-10).attr('y',30);
      else label.attr('transform','rotate(-90)')
        .attr('x',-10).attr('y',10-a.padding[0]);
    }
    return scale;
  });
};

this.hist = function(data) {
  const s = this.scales;
  const g = this.svg.append('g');
  g.selectAll('g.bin').data(data).enter()
    .append('g').call(g => {
      g.append("line").attrs(d => ({
        x1: s[0](d[0]),
        x2: s[0](d[1]),
        y1: s[1](d[2]),
        y2: s[1](d[2])
      }));
      g.filter(d => d[3]).append("line").attrs(d => ({
        x1: s[0]((d[0]+d[1])/2),
        x2: s[0]((d[0]+d[1])/2),
        y1: s[1](d[2]+d[3]),
        y2: s[1](d[2]-(d[4]!=null ? d[4] : d[3])),
      }));
    });
  return g;
};

this.hline = function(y) {
  const x = this.scales[0].range();
  y = this.scales[1](y);
  const g = this.svg.append('g');
  g.append('line').attrs({
    x1: x[0], x2: x[1], y1: y, y2: y
  });
  return g;
};

this.band = function(data) {
  let points = [ ];
  const n = data.bins.length;
  for (let i=0; i<n; ++i) {
    points.push([data.edges[i  ],data.bins[i][0]]);
    points.push([data.edges[i+1],data.bins[i][0]]);
  }
  for (let i=n-1; i>=0; --i) {
    points.push([data.edges[i+1],data.bins[i][1]]);
    points.push([data.edges[i  ],data.bins[i][1]]);
  }
  const s = this.scales;
  const g = this.svg.append('g');
  g.append('polygon').attr('points',
    points.map(p => p.map((x,i) => s[i](x)).join(',')).join(' ')
  );
  return g;
};

this.band_curve = function(points) {
  const s = this.scales;
  const n = points.length;
  const ps = [ ];
  for (let i=0; i<n; ++i) ps.push([points[i][0],points[i][1]+points[i][2]]);
  for (let i=n-1; i>=0; --i) ps.push([points[i][0],points[i][1]-points[i][2]]);
  const g = this.svg.append('g');
  g.append('path').attr('d',
    d3.line().curve(d3.curveCardinal)(ps.map(
      p => [ s[0](p[0]), s[1](p[1]) ]
    )) );
  return g;
};

this.curve = function(points) {
  const s = this.scales;
  const g = this.svg.append('g');
  g.append('path').attr('d',
    d3.line().curve(d3.curveCardinal)(points.map(
      p => [ s[0](p[0]), s[1](p[1]) ]
    )) );
  return g;
};

this.fcurve = function(args) {
  const points = [];
  const d = (args.b-args.a)/(args.n-1);
  for (let i=0; i<args.n; ++i) {
    const x = args.a+i*d;
    points.push([x,args.f(x)]);
  }
  return this.curve(points);
};

this.hist_yrange = function(ys,logy) {
  let min = Number.MAX_VALUE;
  let max = (logy ? 0 : Number.MIN_VALUE);

  for (let y of ys) {
    if (logy && y<=0) continue;
    if (y<min) min = y;
    if (y>max) max = y;
  }

  if (logy) return [
    Math.pow(10,1.05*Math.log10(min) - 0.05*Math.log10(max)),
    Math.pow(10,1.05*Math.log10(max) - 0.05*Math.log10(min))
  ];
  else {
    let both = false;
    if (min > 0.) {
      if (min/max < 0.25) {
        min = 0.;
        max /= 0.95;
      } else both = true;
    } else if (max < 0.) {
      if (min/max < 0.25) {
        max = 0.;
        min /= 0.95;
      } else both = true;
    } else if (min==0.) {
      max /= 0.95;
    } else if (max==0.) {
      min /= 0.95;
    } else both = true;
    if (both) {
      return [ 1.05556*min - 0.05556*max, 1.05556*max - 0.05556*min ];
    }
  }
  return [ min, max ];
};

}
