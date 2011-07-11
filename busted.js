var use_buffers = true, gzip_headers = false, compression_level = 9;


var Gzip = require('compress').Gzip,
    Gunzip = require('compress').Gunzip,
    succeeded = 0, failed = 0, total = 1000000, concurrency = 200,
    original = new Buffer([0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
                           17,18,19,20,21,22,23,24,25,26,27,28.29,30,
                           32,33,34,35,36,37,38,39,40,41,42,
                           0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
                           17,18,19,20,21,22,23,24,25,26,27,28.29,30,
                           32,33,34,35,36,37,38,39,40,41,42]);

console.log("Original length: " + original.length);

function comp_buff(b1,b2) {
  if(b1.length != b2.length) return false;
  for(var i=0; i<b1.length; i++)
    if(b1[i] != b2[i]) return false;
  return true;
}

function test(seq) {
  var   gzip = new Gzip(compression_level, use_buffers, gzip_headers),
      gunzip = new Gunzip(use_buffers, gzip_headers);
  //console.log(original);
  gzip.write(original,
    function(err, d1) {
      gzip.close(
        function(err, d2) {
          var compressed = d1;
          if(d2) {
            compressed = new Buffer(d1.length + d2.length);
            d1.copy(compressed, 0);
            d2.copy(compressed, d1.length);
          }
          if(compressed.length > original.length)
            console.log("Compressed length: " + compressed.length);
          //console.log(compressed);
  
          gunzip.write(compressed,
            function(err, i1) {
              gunzip.close(
                function(err, i2) {
                  var uncompressed = i1;
                  if(i2 && i2.length) {
                    uncompressed = new Buffer(i1.legnth + i2.length);
                    i1.copy(uncompressed, 0);
                    i2.copy(uncompressed, i1.length);
                  }
                  if(uncompressed.length != original.length)
                    console.log("Uncompressed length: " + uncompressed.length);
                  //console.log(uncompressed);
                  if(! comp_buff(original, uncompressed)) {
                    failed++;
                    console.log("Failed");
                  } else {
                    succeeded++;
                  }
                  if(seq > 1) test(seq-1);
                });
            });
        });
  });
}
var last = 0, lasttime = +(new Date());
function report () {
  var current = succeeded + failed,
      now = +(new Date());
  console.log("Successes: " + succeeded + "\nFailures: " + failed +
              "\nRate: " + 1000 * (current - last) / (now - lasttime) + " cycles/second\n");
  last = current;
  lasttime = now;
  if(current == total) return;
  setTimeout(report, 5000);
}

report();
for(i=0; i<concurrency; i++) {
  test(total/concurrency);
}
