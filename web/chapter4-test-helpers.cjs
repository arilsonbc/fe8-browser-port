const {setup,assert,fs,path}=require('./chapter3-test-helpers.cjs');
async function chapter4(difficulty=1){const q=await setup(difficulty),{m}=q;const scripts=JSON.parse(fs.readFileSync(path.join(__dirname,'generated/event_assets.json')));assert.equal(m._fe8_event_call(scripts.find(s=>s.name==='EventScr_Ch3_EndingScene').address,3),0);q.drain();assert.equal(m._fe8_event_info(1),-1);assert.equal(m._fe8_map_info(0),4);return q;}
module.exports={chapter4,assert,fs,path};
