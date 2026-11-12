"""Read-only proof that the native ordinary HeroUseItem accepts a nullable target. No game is run."""
from pathlib import Path
import runpy
abi=runpy.run_path(str(Path(__file__).with_name('test_quest_return_layout.py')))
read,relative=abi['read'],abi['relative']
assert read(0x140429794,7).hex()=='48899530020000' # save RDX hero, without requiring non-null
assert read(0x1404297c0,12).hex()=='488b85300200004885c07428' # test hero, skip hero-only setup if NULL
assert read(0x1404297f4,5).hex()=='4088742430'
assert relative(0x140429816,'e8',5)==0x14076e870 # native overlay admission, target passed unchanged in R8
print('PASS combat item ABI: nullable native hero target skips actor-only preparation; ordinary admission keeps native target; no new hook or game execution')
