function $(s) {
    return document.querySelector(s);
}

function $$(s) {
    return document.querySelectorAll(s);
}

function fetch(url, callback) {
    var xhr = new XMLHttpRequest();
    xhr.addEventListener('load', function() {
        callback(xhr.responseText);
    });
    xhr.open('GET', url, true);
    xhr.send();
}

function identity(x) {
    return x;
}

function Database(csv) {
    this.db = {};
    this.subreddits = [];
    this.casefix = {};
    var lines = csv.split(/\n/);
    for (var i = 0; i < lines.length; i++) {
        var parts = lines[i].split(/,/);
        var sr = parts[0];
        this.subreddits.push(sr);
        this.db[sr.toLowerCase()] = parts.slice(1).filter(identity);
        this.casefix[sr.toLowerCase()] = sr;
    }
}

Database.prototype.lookup = function(sr) {
    var result = {
        to: this.db[sr.toLowerCase()],
        from: null
    };
    if (result.to == null)
        return null;
    var table = {};
    for (var i = 0; i < this.subreddits.length; i++) {
        var from = this.subreddits[i];
        var index = this.db[from.toLowerCase()].indexOf(sr);
        if (index >= 0) {
            if (table[from] == null)
                table[from] = 0;
            table[from] += index;
        }
    }
    var listing = [];
    var keys = Object.keys(table);
    for (var i = 0; i < keys.length; i++) {
        var from = keys[i];
        listing.push({from: from, count: table[from]});
    }
    result.from = listing.sort(function(a, b) {
        return a.count - b.count;
    }).slice(0, 20).map(function(x) {
        return x.from;
    });
    return result;
};

Database.prototype.fix = function(sr) {
    return this.casefix[sr.toLowerCase()] || sr;
};

function update() {
    var srout = $('#sr-out');
    var srin = $('#sr-in');
    srout.innerHTML = '';
    srin.innerHTML = '';
    var sr = db.fix($('#subreddit').value);
    var result = db.lookup(sr);
    [].forEach.call($$('.sr'), function(s) {
        s.textContent = sr;
    });
    if (result) {
        location.replace('#' + sr);
        for (var i = 0; i < result.to.length; i++) {
            var item = document.createElement('div');
            item.className = 'result';
            item.addEventListener('click', linkto);
            item.textContent = result.to[i];
            srout.appendChild(item);
        }
        for (var i = 0; i < result.from.length; i++) {
            var item = document.createElement('div');
            item.className = 'result';
            item.addEventListener('click', linkto);
            item.textContent = result.from[i];
            srin.appendChild(item);
        }
    }
}

function linkto() {
    var sr = this.textContent;
    $('#subreddit').value = sr;
    update();
}

var db = null;
fetch('current.csv', function(csv) {
    db = new Database(csv);
    var loading = $('#loading');
    loading.parentNode.removeChild(loading);
    var text = $('#subreddit');
    text.addEventListener('keyup', update);
    text.addEventListener('change', update);
    text.value = location.hash ? location.hash.slice(1) : 'datasets';
    text.focus();
    text.select();
    update();
});
