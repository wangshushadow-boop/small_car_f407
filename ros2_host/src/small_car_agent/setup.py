from setuptools import find_packages, setup

package_name = "small_car_agent"

setup(
    name=package_name,
    version="0.1.0",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages", ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="small_car",
    maintainer_email="small-car@example.com",
    description="为 LangGraph 小车大脑提供音视频感知快照。",
    license="Apache-2.0",
    entry_points={
        "console_scripts": [
            "perception_snapshot = small_car_agent.perception_tool:main",
            "agent_event_node = small_car_agent.agent_event_node:main",
        ],
    },
)
