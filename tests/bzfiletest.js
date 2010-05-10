var compress=require("../compress-bindings");
var sys=require("sys");
var posix=require("fs");
var Buffer = require('buffer').Buffer;

function createBuffer(str, enc) {
  enc = enc || 'utf8';
  var len = Buffer.byteLength(str, enc);
  var buf = new Buffer(len);
  buf.write(str, enc, 0);
  return buf;
}


// Read in our test file
var data=posix.readFileSync("tests/bzfiletest.js", encoding="binary");
sys.puts("Got : "+data.length);

// Set output file
var fd = posix.openSync("bzfiletest.js.bz2",
    process.O_WRONLY | process.O_TRUNC | process.O_CREAT, 0644);
sys.puts("Openned file");

// Create bzip stream
var bzip=new compress.Bzip();

// Pump data to be compressed
var bzdata, bzlast;
bzip.write(createBuffer(data, "binary"), function(err, data) {
  bzdata = data;
});
sys.puts("Compressed size : "+bzdata.length);
posix.writeSync(fd, bzdata, encoding="binary");

// Get the last bit

bzip.close(function(err, data) {
  bzlast = data;
});
sys.puts("Last bit : "+bzlast.length);
posix.writeSync(fd, bzlast, encoding="binary");
posix.closeSync(fd);
sys.puts("File closed");

// See if we can uncompress it ok
var source;
var bunzip=new compress.Bunzip;
var testdata = posix.readFileSync("bzfiletest.js.bz2", encoding="binary");
sys.puts("Test opened : "+testdata.length);
bunzip.write(createBuffer(testdata, "binary"), function(err, data) {
  source = data;
});
bunzip.close(function(err, data) {
  source += data;
});
sys.puts(source.length);
sys.puts(source);






