const fs = require('fs');
const html = fs.readFileSync('index.html', 'utf8');
console.log("Does it contain setSafeHTML?", html.includes('setSafeHTML'));
console.log("Does it contain function setSafeHTML?", html.includes('function setSafeHTML'));
console.log("Does it contain const setSafeHTML?", html.includes('const setSafeHTML'));
