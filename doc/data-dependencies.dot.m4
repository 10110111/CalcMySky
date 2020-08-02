digraph calculation
{
    overlap=false;
    graph [pad="0.5", ranksep="0.8"];
    define(`calculation', `shape=rectangle, color=pink')
    define(`for_current_scatterer', `color=orange')
    define(`blended_from_loop', `color="black:invis:black"')
    define(`output', `color=aquamarine2')
    define(`multiple_elements', `peripheries=2')
    define(`note', `<font point-size="11">$1</font>')
    define(`data_color', `lightblue2')

    size="14,14";
    node [color=data_color, style=filled];

    transmittanceCalc [calculation, label="transmittance"];
    subgraph cluster_0
    {
        label="All scatterers at once";
        color=data_color;
        allScattererDensities [label="number densities"];
        allScatteringCrossSections [label="cross sections"];
        allPhaseFunctions [label="phase functions"];
    }
    subgraph cluster_1
    {
        label="All absorbers at once";
        color=data_color;
        allAbsorberDensities [label="number densities"];
        allAbsorptionCrossSections [label="cross sections"];
    }
    transmittanceTexture [output, label="transmittance (2D)"];
    allScattererDensities -> transmittanceCalc;
    allScatteringCrossSections -> transmittanceCalc;
    allAbsorberDensities -> transmittanceCalc;
    allAbsorptionCrossSections -> transmittanceCalc;
    transmittanceCalc -> transmittanceTexture;

    directIrradianceCalc [calculation, label="direct irradiance"];
    directIrradianceTexture [label=<direct irradiance (2D)<br/>(saved in delta irradiance texture)>];
    irradianceTexture [output, label=<irradiance (2D)<br/>note(used only to render ground or measure irradiances)>];
    transmittanceTexture -> directIrradianceCalc;
    directIrradianceCalc -> directIrradianceTexture;
    directIrradianceCalc -> irradianceTexture;

    // --------- scattering orders 1 and 2 ------------------
    scatteringDensityOrder2FromGround [calculation, label=<scattering density order 2:<br/>from ground>];
    directIrradianceTexture -> scatteringDensityOrder2FromGround;
    transmittanceTexture -> scatteringDensityOrder2FromGround;
    transmittanceTexture -> singleScatteringCalc;
    subgraph cluster_2
    {
        color=blue;
        label="Looping over scatterers";
        subgraph cluster_3
        {
            label="Current scatterer";
            for_current_scatterer;
            currentScattererDensity [for_current_scatterer, label="number density"];
            currentScattererCrossSection [for_current_scatterer, label="cross section"];
            currentScattererPhaseFunction [for_current_scatterer, label="phase function"];
        }
        singleScatteringCalc [calculation, label="single scattering"];
        firstScattering [label=<first scattering (4D)<br/>(saved in delta scattering texture)<br/>note(phase functions aren't included)>];
        indirectIrradianceOrder1Calc [calculation, label="indirect irradiance order 1"];
        scatteringDensityOrder2FromAir [calculation, label=<scattering density order 2:<br/>from current scatterer in air>];
    }
    allPhaseFunctions -> currentScattererPhaseFunction;
    allScattererDensities -> currentScattererDensity;
    allScatteringCrossSections -> currentScattererCrossSection;
    currentScattererPhaseFunction -> scatteringDensityOrder2FromAir;
    currentScattererDensity -> singleScatteringCalc;
    currentScattererCrossSection -> singleScatteringCalc;
    singleScatteringCalc -> firstScattering;
    firstScattering -> scatteringDensityOrder2FromAir;
    firstScattering -> indirectIrradianceOrder1Calc;

    deltaIrradianceTexture_node1 [label="delta irradiance (2D)"];
    indirectIrradianceOrder1Calc -> deltaIrradianceTexture_node1 [blended_from_loop];
    indirectIrradianceOrder1Calc -> irradianceTexture [blended_from_loop];
    singleScattering [output, multiple_elements, label=<single scattering textures (4D)<br/>(one texture per scatterer)<br/>note(phase functions aren't included)>];
    singleScatteringCalc -> singleScattering;
    singleScatteringCalc -> singleScattering;
    singleScatteringCalc -> singleScattering;
    deltaScatteringDensityTexture_node1 [label=<delta scattering density (4D)<br/>note(includes phase functions)>];
    scatteringDensityOrder2FromGround -> deltaScatteringDensityTexture_node1;
    scatteringDensityOrder2FromAir -> deltaScatteringDensityTexture_node1 [blended_from_loop];

    multipleScatteringOrder2Calc [calculation, label="multiple scattering order 2"];
    transmittanceTexture -> multipleScatteringOrder2Calc;
    deltaScatteringDensityTexture_node1 -> multipleScatteringOrder2Calc;
    deltaMultipleScatteringTexture_node1 [label="delta multiple scattering (4D)"];
    multipleScatteringOrder2Calc -> deltaMultipleScatteringTexture_node1;
    multipleScatteringOrder2Calc -> scatteringTexture;

    // --------- scattering order ≥ 3 ------------------
    allPhaseFunctions -> scatteringDensityHigherOrder;
    allScattererDensities -> scatteringDensityHigherOrder;
    subgraph cluster_4
    {
        color=blue;
        label="Looping over scattering orders ≥ 3";
        scatteringDensityHigherOrder [calculation, label="scattering density order ≥ 3"];
        deltaScatteringDensityTexture_node2 [label=<delta scattering density (4D)<br/>note(includes phase functions)>];
        multipleScatteringHigherOrderCalc [calculation, label="multiple scattering order ≥ 3"];
        indirectIrradianceHigherOrderCalc [calculation, label="indirect irradiance order ≥ 2"];
    }
    transmittanceTexture -> scatteringDensityHigherOrder;
    deltaIrradianceTexture_node1 -> scatteringDensityHigherOrder;
    deltaMultipleScatteringTexture_node1 -> scatteringDensityHigherOrder;
    scatteringDensityHigherOrder -> deltaScatteringDensityTexture_node2;

    deltaMultipleScatteringTexture_node1 -> indirectIrradianceHigherOrderCalc;
    deltaIrradianceTexture_node2 [label="delta irradiance (2D)"];
    indirectIrradianceHigherOrderCalc -> deltaIrradianceTexture_node2;
    indirectIrradianceHigherOrderCalc -> irradianceTexture [blended_from_loop];

    {rank=same; deltaIrradianceTexture_node1; deltaIrradianceTexture_node2};
    deltaIrradianceTexture_node2 ->
        deltaIrradianceTexture_node1
            [style=dashed, color=data_color]; // show that these nodes are the same texture and the direction of data use

    transmittanceTexture -> multipleScatteringHigherOrderCalc;
    deltaScatteringDensityTexture_node2 -> multipleScatteringHigherOrderCalc;
    deltaMultipleScatteringTexture_node2 [label="delta multiple scattering (4D)"];
    multipleScatteringHigherOrderCalc -> deltaMultipleScatteringTexture_node2;
    scatteringTexture [output, label="multiple scattering (4D)"];
    multipleScatteringHigherOrderCalc -> scatteringTexture [blended_from_loop];

    {rank=same; deltaMultipleScatteringTexture_node1; deltaMultipleScatteringTexture_node2};
    deltaMultipleScatteringTexture_node2 ->
        deltaMultipleScatteringTexture_node1
            [style=dashed, color=data_color, weight="500"]; // show that these nodes are the same texture and the direction of data use
}
