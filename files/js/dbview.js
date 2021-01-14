function DBView(args) {
// args: div, dbs, default_selection, process_data
const cache = { };

const default_selection = args.default_selection || ((col,i,x) => i==0);

const div_id = args.div.prop('id');
const form = $('<form>').appendTo(args.div);
const db_div = $('<div id="'+div_id+'_db">').appendTo(form);
const labels_div = $('<div id="'+div_id+'_labels">').appendTo(form);

function hide_while_loading(loading) {
  form.find(':input').prop("disabled",loading);
  form.find('.loading').toggle(loading);
}

function load(url,data) {
  return $.ajax({
    type: 'POST',
    url: url,
    data: data,
    beforeSend: hide_while_loading.bind(null,true),
    dataType: 'text',
    dataFilter: function(resp) {
      if (resp.length) {
        try {
          return JSON.parse(resp);
        } catch(e) {
          alert('Bad server response: '+resp);
          console.log(resp);
        }
      } else alert('Empty server response');
      hide_while_loading(false);
      return false;
    },
    success: hide_while_loading.bind(null,false)
  });
}

function load_labels(dbname) {
  const notes = $('#notes');
  if (notes.length==1) {
    notes.empty();
    $.post('hist_notes',
      {'note': dbname},
      function(note) {
        if (note.length) {
          notes.append(note);
          $("#show_notes").show();
        } else {
          $("#show_notes").hide();
        }
      });
  }
  return load('hist/data/'+dbname+'.cols').done(function(resp){
    labels_div.empty();
    const cols = [ ];
    for (const col of resp.cols) {
      const div = $('<div>').appendTo(labels_div);
      $('<div>').appendTo(div).text(col[0]).addClass('label_name');
      // const sel = $('<select>').appendTo(div);
      const sel = $('<select>').appendTo(
        $('<div>').appendTo(div).addClass('select'));
      cols.push(sel[0]);
      sel.attr({ name: col[0], size: 10, multiple: '' })
      .append(col[1].map((x,i) => {
        const opt = $('<option>').text(x);
        if (default_selection(col[0],i,x)) opt.attr('selected','');
        return opt;
      }))
      .change(function(){
        const xs1 = $(this).val();
        if (xs1.length<1) return;
        const v1 = this.name;
        const v2 = resp.vals[v1];
        if (v2) {
          const xs2 = [ ]; // accumulate unique values
          for (const x1 of xs1) {
            for (const x2 of v2[1][x1])
              if (!xs2.includes(x2)) xs2.push(x2);
          }
          const s2 = labels_div.find('[name='+v2[0]+']');
          const prev = s2.val();
          xs2.reduce((s,x) => s.append($('<option>').text(x)), s2.empty());
          if (xs2.length==1 && xs2[0].length==0) {
            s2.val('').hide();
          } else {
            s2.val(prev).show();
          }
        }
        if (cols.find(s => {
          const n = s.options.length;
          for (let i=0; i<n; ++i)
            if (s.options[i].selected) return false;
          return true;
        })) return;
        const selection = { };
        labels_div.find('[name]').each(
          (i,x) => { selection[x.name] = $(x).val() });
        load_data(sel,{ db: dbname, labels: selection });
      });
    }
  });
}

function load_data(sel,req) {
  const req_url = ([['db',[req.db]]].concat(Object.entries(req.labels)))
    .reduce((a,x) =>
        a + '&'+encodeURIComponent(x[0])
          + '='+x[1].map(encodeURIComponent).join('+'), '');
  const full_url = window.location.href.split('?')[0]+'?page='+page+req_url;

  args.div.find('.share > a').prop('href',full_url);
  args.div.find('.share > form').show().find('[name="url"]')
    .prop('value',full_url);
  if (window.history.replaceState)
    window.history.replaceState(null,'Histograms',full_url);
  if (req_url in cache) {
    args.process_data(req,cache[req_url]);
  } else {
    return load('hist_req',req).done(function(resp){
      cache[req_url] = resp;
      args.process_data(req,resp);
      sel.focus();
    });
  }
}

$('<select>').appendTo(db_div).prop('name','db')
.append([''].concat(args.dbs).map(x => $('<option>').text(x)))
.change(function(){
  const sel = $(this);
  const name = this.value;
  if (name!=='') {
    sel.nextAll('.hint').hide();
    sel.nextAll('.share').show();
    return load_labels(name);
  } else {
    sel.nextAll('.hint').show();
    sel.nextAll('.share').hide();
  }
})
.after($('<img>').prop({
  src: 'icons/loading.gif',
  alt: 'loading'
}).css({
  display: 'none',
  'vertical-align': 'middle'
}).addClass('loading')).after(
  $('<span>').addClass('share').css({
    display: 'none'
  }).append(
    $('<a>').prop({
      href: '?page='+page
    }).append($('<img>').prop({
      src: 'img/icons/share.svg',
      alt: 'share',
      height: 16
    }).css({
      'vertical-align': 'middle'
    })).append('link to this selection')
  // ).append(
  //   [{type:'hidden',name:'source',value:'index'},
  //    {type:'hidden',name:'url',value:'?page='+page},
  //    {type:'hidden',name:'alias',value:''},
  //    {type:'submit',value:'TinyURL'}
  //   ].reduce((a,x) => a.append($('<input>').prop(x)),
  //     $('<form>').prop({
  //       action: 'https://tinyurl.com/create.php',
  //       method: 'get', name: 'f', target:'_blank'
  //     })
  //   ).addClass('inline_form').hide()
  )
)
.after($('<span>').addClass('hint').text('← select plot set'));

// load from URL
const plot_arg = { };
window.location.href.replace(/[?&]+([^=&]+)=([^&]*)/gi,
  function(m,key,value) {
    if (key!='page')
      plot_arg[decodeURIComponent(key)] = decodeURIComponent(value).split('+');
  });
if (Object.keys(plot_arg).length) {
  const db = plot_arg.db[0];
  delete plot_arg.db;
  if (args.dbs.includes(db))
    form.find('[name=db]').val(db).triggerHandler('change')
    .done(function(resp){
      for (let name in plot_arg) {
        const vals = plot_arg[name];
        form.find('[name='+name+']').children().each( (i,x) => {
          x.selected = vals.includes(x.value);
        });
      }
      const sels = form.find('select');
      if (sels.toArray().findIndex(
        s => Array.from(s.childNodes).findIndex(
          opt => (opt).selected) == -1) == -1)
      {
        sels.last().trigger('change');
      }
    });
}
}
