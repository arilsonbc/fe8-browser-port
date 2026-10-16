const {chapter4,assert,fs,path}=require('./chapter4-test-helpers.cjs');
async function chapter5(difficulty=1){const q=await chapter4(difficulty),scripts=JSON.parse(fs.readFileSync(path.join(__dirname,'generated/event_assets.json')));assert.equal(q.m._fe8_event_call(scripts.find(s=>s.name==='EventScr_089F19F8').address,3),0);q.drain();assert.equal(q.m._fe8_event_info(1),-1);assert.equal(q.m._fe8_map_info(0),6);return q;}
module.exports={chapter5,assert,fs,path};
