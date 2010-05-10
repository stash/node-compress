var compress=require("./compress");
var sys=require("sys");
var posix=require("fs");

// Read in our test file
var data=posix.readFileSync("filetest.js", encoding="binary");
sys.puts("Got : "+data.length);

// Set output file
var fd = posix.openSync("filetest.js.gz",
    process.O_WRONLY | process.O_TRUNC | process.O_CREAT, 0644);
sys.puts("Openned file");

// Create gzip stream
var gzip=new compress.Gzip;
gzip.init();

// Pump data to be compressed
gzdata=gzip.deflate(data, "binary");  // Do this as many times as required
sys.puts("Compressed size : "+gzdata.length);
posix.writeSync(fd, gzdata, encoding="binary");

// Get the last bit
gzlast=gzip.end();
sys.puts("Last bit : "+gzlast.length);
posix.writeSync(fd, gzlast, encoding="binary");
posix.closeSync(fd);
sys.puts("File closed");

// See if we can uncompress it ok
var gunzip=new compress.Gunzip;
gunzip.init();
var testdata = posix.readFileSync("filetest.js.gz", encoding="binary");
sys.puts("Test opened : "+testdata.length);
var source = gunzip.inflate(testdata, "binary");
sys.puts(source.length);
sys.puts(source);
gunzip.end();






